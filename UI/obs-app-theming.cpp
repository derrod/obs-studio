/******************************************************************************
    Copyright (C) 2023 by Dennis Sädtler <dennis@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <cinttypes>

#include <util/cf-parser.h>

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QMetaEnum>
#include <QDirIterator>
#include <QGuiApplication>
#include <QRandomGenerator>

#include "qt-wrappers.hpp"
#include "obs-app.hpp"
#include "obs-app-theming.hpp"
#include "obs-proxy-style.hpp"
#include "platform.hpp"

#include "ui-config.h"

using namespace std;

struct CFParser {
	cf_parser cfp = {};
	~CFParser() { cf_parser_free(&cfp); }
	operator cf_parser *() { return &cfp; }
	cf_parser *operator->() { return &cfp; }
};

static OBSTheme *ParseThemeMeta(const QString &path)
{
	QFile themeFile(path);
	if (!themeFile.open(QIODeviceBase::ReadOnly))
		return nullptr;

	OBSTheme *meta = nullptr;
	const QByteArray data = themeFile.readAll();
	CFParser cfp;
	int ret;

	if (!cf_parser_parse(cfp, data.constData(), QT_TO_UTF8(path)))
		return nullptr;

	if (cf_token_is(cfp, "OBSThemeMeta") ||
	    cf_go_to_token(cfp, "OBSThemeMeta", nullptr)) {

		if (!cf_next_token(cfp))
			return nullptr;

		if (!cf_token_is(cfp, "{"))
			return nullptr;

		meta = new OBSTheme();

		for (;;) {
			if (!cf_next_token(cfp)) {
				delete meta;
				return nullptr;
			}

			ret = cf_token_is_type(cfp, CFTOKEN_NAME, "name",
					       nullptr);
			if (ret != PARSE_SUCCESS)
				break;

			string name(cfp->cur_token->str.array,
				    cfp->cur_token->str.len);

			ret = cf_next_token_should_be(cfp, ":", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;

			if (!cf_next_token(cfp)) {
				delete meta;
				return nullptr;
			}

			ret = cf_token_is_type(cfp, CFTOKEN_STRING, "value",
					       ";");

			if (ret != PARSE_SUCCESS)
				continue;

			BPtr str = cf_literal_to_str(cfp->cur_token->str.array,
						     cfp->cur_token->str.len);

			if (str) {
				if (name == "dark")
					meta->isDark = strcmp(str, "true") == 0;
				else if (name == "extends")
					meta->extends = str;
				else if (name == "author")
					meta->author = str;
				else if (name == "id")
					meta->id = str;
				else if (name == "name")
					meta->name = str;
			}

			if (!cf_go_to_token(cfp, ";", nullptr)) {
				delete meta;
				return nullptr;
			}
		}
	}

	if (meta) {
		meta->isBaseTheme = path.endsWith(".obt");

		if (meta->id.isEmpty() || meta->name.isEmpty() ||
		    (!meta->isBaseTheme && meta->extends.isEmpty())) {
			/* Theme is invalid */
			delete meta;
			meta = nullptr;
		} else {
			BPtr absPath = os_get_abs_path_ptr(QT_TO_UTF8(path));
			meta->location = absPath;
			meta->isHighContrast = path.endsWith(".oha");
			meta->isVisible = !path.contains("System");
		}
	}

	return meta;
}

static QColor ParseColour(CFParser &cfp)
{
	const char *array;
	uint32_t color = 0;
	QColor res(QColor::Invalid);

	if (cf_token_is(cfp, "#")) {
		if (!cf_next_token(cfp))
			return res;

		color = strtol(cfp->cur_token->str.array, nullptr, 16);
	} else if (cf_token_is(cfp, "rgb")) {
		int ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return res;

		array = cfp->cur_token->str.array;
		color |= strtol(array, nullptr, 10) << 16;

		ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return res;

		array = cfp->cur_token->str.array;
		color |= strtol(array, nullptr, 10) << 8;

		ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return res;

		array = cfp->cur_token->str.array;
		color |= strtol(array, nullptr, 10);

		ret = cf_next_token_should_be(cfp, ")", ";", nullptr);
		if (ret != PARSE_SUCCESS)
			return res;
	}

	res = color;
	return res;
}

static bool ParseCalc(CFParser &cfp, QStringList &calc,
		      vector<OBSThemeVariable> &vars)
{
	int ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
	if (ret != PARSE_SUCCESS)
		return false;
	if (!cf_next_token(cfp))
		return false;

	while (!cf_token_is(cfp, ")")) {
		if (cf_token_is(cfp, ";"))
			break;

		if (cf_token_is(cfp, "calc")) {
			/* Internal calc's do not have proper names,
			 * they are anonymous variables */
			OBSThemeVariable var;
			QStringList subcalc;

			var.name = QString("__unnamed_%1")
					   .arg(QRandomGenerator::global()
							->generate64());

			if (!ParseCalc(cfp, subcalc, vars))
				return false;

			var.type = OBSThemeVariable::Calc;
			var.value = subcalc;
			calc << var.name;
			vars.push_back(std::move(var));
		} else {
			calc << QString::fromUtf8(cfp->cur_token->str.array,
						  cfp->cur_token->str.len);
		}

		if (!cf_next_token(cfp))
			return false;
	}

	return !calc.isEmpty();
}

static bool ParseRange(CFParser &cfp, OBSThemeVariable &var)
{
	int ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
	if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
		return false;

	const char *array = cfp->cur_token->str.array;
	var.value = os_strtod(array);

	ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
	if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
		return false;

	array = cfp->cur_token->str.array;
	var.rangeMin = os_strtod(array);

	ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
	if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
		return false;

	array = cfp->cur_token->str.array;
	var.rangeMax = os_strtod(array);

	/* At this point everything is optional */
	ret = cf_next_token_should_be(cfp, ",", nullptr, nullptr);
	if (ret != PARSE_SUCCESS)
		return true;
	if (!cf_next_token(cfp))
		return false;

	array = cfp->cur_token->str.array;
	var.rangeStep = os_strtod(array);

	ret = cf_next_token_should_be(cfp, ",", nullptr, nullptr);
	if (ret != PARSE_SUCCESS)
		return true;
	if (!cf_next_token(cfp))
		return false;

	var.suffix = QString::fromUtf8(cfp->cur_token->str.array,
				       cfp->cur_token->str.len);

	ret = cf_next_token_should_be(cfp, ")", ";", nullptr);
	if (ret != PARSE_SUCCESS)
		return false;

	return true;
}

static bool ParsePresets(CFParser &cfp, OBSThemeVariable &var)
{
	int ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
	if (ret != PARSE_SUCCESS)
		return false;
	if (!cf_next_token(cfp))
		return false;

	while (!cf_token_is(cfp, ")")) {
		if (cf_token_is(cfp, ";"))
			break;

		/* Default value */
		cf_token_type type = cfp->cur_token->type;
		if (type == CFTOKEN_NAME && !var.value.isValid()) {
			var.value = QString::fromUtf8(cfp->cur_token->str.array,
						      cfp->cur_token->str.len);
			ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
			if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
				return false;
			continue;
		}

		QString key = QString::fromUtf8(cfp->cur_token->str.array,
						cfp->cur_token->str.len);
		ret = cf_next_token_should_be(cfp, ",", ";", nullptr);
		if (ret != PARSE_SUCCESS || !cf_next_token(cfp))
			return false;

		BPtr str = cf_literal_to_str(cfp->cur_token->str.array,
					     cfp->cur_token->str.len);
		QString value = QString::fromUtf8(str.Get());
		var.presets.emplaceBack(make_pair(key, value));

		ret = cf_next_token_should_be(cfp, ",", nullptr, nullptr);
		if (ret != PARSE_SUCCESS)
			break;
		if (!cf_next_token(cfp))
			return false;
	}

	return !var.presets.isEmpty();
}

static vector<OBSThemeVariable> ParseThemeVariables(const char *themeData)
{
	CFParser cfp;
	int ret;

	std::vector<OBSThemeVariable> vars;

	if (!cf_parser_parse(cfp, themeData, nullptr))
		return vars;

	if (!cf_token_is(cfp, "OBSThemeVars") &&
	    !cf_go_to_token(cfp, "OBSThemeVars", nullptr))
		return vars;

	if (!cf_next_token(cfp))
		return {};

	if (!cf_token_is(cfp, "{"))
		return {};

	for (;;) {
		if (!cf_next_token(cfp))
			return vars;

		ret = cf_token_is_type(cfp, CFTOKEN_NAME, "key", nullptr);
		if (ret != PARSE_SUCCESS)
			break;

		QString key = QString::fromUtf8(cfp->cur_token->str.array,
						cfp->cur_token->str.len);
		OBSThemeVariable var;
		var.name = key;

		ret = cf_next_token_should_be(cfp, ":", ";", nullptr);
		if (ret != PARSE_SUCCESS)
			continue;

		if (!cf_next_token(cfp))
			return vars;

		if (cfp->cur_token->type == CFTOKEN_NUM) {
			const char *ch = cfp->cur_token->str.array;
			const char *end = ch + cfp->cur_token->str.len;
			double f = os_strtod(ch);

			var.value = f;
			var.type = OBSThemeVariable::Number;

			/* Look for a suffix and mark variable as size if it exists */
			while (ch < end) {
				if (!isdigit(*ch) && !isspace(*ch)) {
					var.suffix =
						QString::fromUtf8(ch, end - ch);
					var.type = OBSThemeVariable::Size;
					break;
				}
				ch++;
			}
		} else if (cf_token_is(cfp, "rgb") || cf_token_is(cfp, "#")) {
			QColor color = ParseColour(cfp);
			if (!color.isValid())
				continue;

			var.value = color;
			var.type = OBSThemeVariable::Color;
		} else if (cf_token_is(cfp, "alias")) {
			ret = cf_next_token_should_be(cfp, "(", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;
			if (!cf_next_token(cfp))
				return vars;

			var.value = QString::fromUtf8(cfp->cur_token->str.array,
						      cfp->cur_token->str.len);
			var.type = OBSThemeVariable::Alias;

			ret = cf_next_token_should_be(cfp, ")", ";", nullptr);
			if (ret != PARSE_SUCCESS)
				continue;
		} else if (cf_token_is(cfp, "calc")) {
			QStringList calc;

			if (!ParseCalc(cfp, calc, vars))
				continue;

			var.type = OBSThemeVariable::Calc;
			var.value = calc;
		} else if (cf_token_is(cfp, "range")) {
			if (!ParseRange(cfp, var))
				continue;

			var.editable = true;
			var.type = OBSThemeVariable::Range;
		} else if (cf_token_is(cfp, "preset")) {
			if (!ParsePresets(cfp, var))
				continue;

			var.editable = true;
			var.type = OBSThemeVariable::Preset;
		} else {
			var.type = OBSThemeVariable::String;
			BPtr strVal =
				cf_literal_to_str(cfp->cur_token->str.array,
						  cfp->cur_token->str.len);
			var.value = QString::fromUtf8(strVal.Get());
		}

		if (!cf_next_token(cfp))
			return vars;

		if (cf_token_is(cfp, "!") &&
		    cf_next_token_should_be(cfp, "editable", nullptr,
					    nullptr) == PARSE_SUCCESS) {
			if (var.type == OBSThemeVariable::Calc ||
			    var.type == OBSThemeVariable::Alias) {
				blog(LOG_WARNING,
				     "Variable of calc/alias type cannot be editable: %s",
				     QT_TO_UTF8(var.name));
			} else {
				var.editable = true;
			}
		}

		vars.push_back(std::move(var));

		if (!cf_token_is(cfp, ";") &&
		    !cf_go_to_token(cfp, ";", nullptr))
			return vars;
	}

	return vars;
}

void OBSApp::ParseThemeCustomisation(QHash<QString, OBSThemeVariable> &vars)
{
	if (!config_get_bool(globalConfig, "Appearance", "Customisation"))
		return;

	QPalette pal = palette();
	QString sectionName = "Appearance.Customisation." + currentTheme->id;

	for (const QString &key : vars.keys()) {
		if (!config_has_user_value(globalConfig,
					   QT_TO_UTF8(sectionName),
					   QT_TO_UTF8(key)))
			continue;

		const char *str;
		double dbl;
		OBSThemeVariable &var = vars[key];

		switch (var.type) {
		case OBSThemeVariable::Alias:
		case OBSThemeVariable::Calc:
			continue;
		case OBSThemeVariable::Size:
		case OBSThemeVariable::Number:
		case OBSThemeVariable::Range:
			dbl = config_get_double(globalConfig,
						QT_TO_UTF8(sectionName),
						QT_TO_UTF8(key));
			var.userValue = dbl;
			break;
		case OBSThemeVariable::Color:
			str = config_get_string(globalConfig,
						QT_TO_UTF8(sectionName),
						QT_TO_UTF8(key));
			var.userValue = QColor(str);
			break;
		case OBSThemeVariable::String:
		case OBSThemeVariable::Preset:
			str = config_get_string(globalConfig,
						QT_TO_UTF8(sectionName),
						QT_TO_UTF8(key));
			var.userValue = str;
			break;
		}

		userVariables[key] = var;
	}
}

void OBSApp::FindThemes()
{
	string themeDir;
	themeDir.resize(512);

	QStringList filters;
	filters << "*.obt" // OBS Base Theme
		<< "*.ovt" // OBS Variant Theme
		<< "*.oha" // OBS High-contrast Adjustment layer
		;

	if (GetConfigPath(themeDir.data(), themeDir.capacity(),
			  "obs-studio/themes/") > 0) {
		QDirIterator it(QT_UTF8(themeDir.c_str()), filters,
				QDir::Files);

		while (it.hasNext()) {
			OBSTheme *theme = ParseThemeMeta(it.next());
			if (theme && !themes.contains(theme->id))
				themes[theme->id] = std::move(*theme);
			else
				delete theme;
		}
	}

	GetDataFilePath("themes/", themeDir);
	QDirIterator it(QString::fromStdString(themeDir), filters, QDir::Files);
	while (it.hasNext()) {
		OBSTheme *theme = ParseThemeMeta(it.next());
		if (theme && !themes.contains(theme->id))
			themes[theme->id] = std::move(*theme);
		else
			delete theme;
	}

	/* Build dependency tree for all themes, removing ones that have items missing. */
	QSet<QString> invalid;

	for (OBSTheme &theme : themes) {
		if (theme.extends.isEmpty()) {
			if (!theme.isBaseTheme) {
				blog(LOG_ERROR,
				     R"(Theme "%s" is not base, but does not specify parent!)",
				     QT_TO_UTF8(theme.id));
				invalid.insert(theme.id);
			}

			continue;
		}

		QString parentId = theme.extends;
		while (!parentId.isEmpty()) {
			OBSTheme *parent = GetTheme(parentId);
			if (!parent) {
				blog(LOG_ERROR,
				     R"(Theme "%s" is missing ancestor "%s"!)",
				     QT_TO_UTF8(theme.id),
				     QT_TO_UTF8(parentId));
				invalid.insert(theme.id);
				break;
			}

			if (theme.isBaseTheme && !parent->isBaseTheme) {
				blog(LOG_ERROR,
				     R"(Ancestor "%s" of base theme "%s" is not a base theme!)",
				     QT_TO_UTF8(parent->id),
				     QT_TO_UTF8(theme.id));
				invalid.insert(theme.id);
				break;
			}

			/* Mark this theme as a variant of first parent that is a base theme. */
			if (!theme.isBaseTheme && parent->isBaseTheme &&
			    theme.parent.isEmpty())
				theme.parent = parent->id;

			theme.dependencies.push_front(parent->id);
			parentId = parent->extends;

			if (parentId.isEmpty() && !parent->isBaseTheme) {
				blog(LOG_ERROR,
				     R"(Final ancestor of "%s" ("%s") is not a base theme!)",
				     QT_TO_UTF8(theme.id),
				     QT_TO_UTF8(parent->id));
				invalid.insert(theme.id);
				break;
			}
		}
	}

	for (const QString &name : invalid) {
		themes.remove(name);
	}
}

static qsizetype FindEndOfOBSMetadata(const QString &content)
{
	/* Find end of last OBS-specific section and strip it, kinda jank but should work */
	qsizetype end = 0;

	for (auto section : {"OBSThemeMeta", "OBSThemeVars", "OBSTheme"}) {
		qsizetype idx = content.indexOf(section, 0);
		if (idx > end) {
			end = content.indexOf('}', idx) + 1;
		}
	}

	return end;
}

static bool ResolveVariable(const QHash<QString, OBSThemeVariable> &vars,
			    OBSThemeVariable &var)
{
	const OBSThemeVariable *varPtr = &var;
	const OBSThemeVariable *realVar = varPtr;

	while (realVar->type == OBSThemeVariable::Alias ||
	       realVar->type == OBSThemeVariable::Preset) {
		QString newKey = realVar->userValue.isValid()
					 ? realVar->userValue.toString()
					 : realVar->value.toString();

		if (!vars.contains(newKey)) {
			blog(LOG_ERROR,
			     R"(Variable "%s" (aliased by "%s") does not exist!)",
			     QT_TO_UTF8(newKey), QT_TO_UTF8(var.name));
			return false;
		}

		const OBSThemeVariable &newVar = vars[newKey];
		realVar = &newVar;
	}

	if (realVar != varPtr)
		var = *realVar;

	return true;
}

static QString EvalCalc(const QHash<QString, OBSThemeVariable> &vars,
			const OBSThemeVariable &var, const int recursion = 0);

static OBSThemeVariable
ParseCalcVariable(const QHash<QString, OBSThemeVariable> &vars,
		  const QString &value, const int recursion = 0)
{
	OBSThemeVariable var;
	const QByteArray utf8 = value.toUtf8();
	const char *data = utf8.constData();

	if (isdigit(*data)) {
		double f = os_strtod(data);
		var.type = OBSThemeVariable::Number;
		var.value = f;

		const char *dataEnd = data + utf8.size();
		while (data < dataEnd) {
			if (*data && !isdigit(*data)) {
				var.suffix =
					QString::fromUtf8(data, dataEnd - data);
				var.type = OBSThemeVariable::Size;
				break;
			}

			data++;
		}
	} else {
		/* Treat value as an alias/key and resolve it */
		var.type = OBSThemeVariable::Alias;
		var.value = value;
		ResolveVariable(vars, var);

		/* Handle nested calc()s */
		if (var.type == OBSThemeVariable::Calc) {
			QString val = EvalCalc(vars, var, recursion + 1);
			var = ParseCalcVariable(vars, val);
		}

		/* Only number or size would be valid here */
		if (var.type != OBSThemeVariable::Number &&
		    var.type != OBSThemeVariable::Size) {
			blog(LOG_ERROR,
			     "calc() operand is not a size or number: %s",
			     QT_TO_UTF8(var.value.toString()));
			throw invalid_argument("Operand not of numeric type");
		}
	}

	return var;
}

static QString EvalCalc(const QHash<QString, OBSThemeVariable> &vars,
			const OBSThemeVariable &var, const int recursion)
{
	if (recursion >= 10) {
		/* Abort after 10 levels of recursion */
		blog(LOG_ERROR, "Maximum calc() recursion levels hit!");
		return "'Invalid expression'";
	}

	QStringList args = var.value.toStringList();
	if (args.length() != 3) {
		blog(LOG_ERROR,
		     "calc() had invalid number of arguments: %lld (%s)",
		     args.length(), QT_TO_UTF8(args.join(", ")));
		return "'Invalid expression'";
	}

	QString &opt = args[1];
	if (opt != '*' && opt != '+' && opt != '-' && opt != '/') {
		blog(LOG_ERROR, "Unknown/invalid calc() operator: %s",
		     QT_TO_UTF8(opt));
		return "'Invalid expression'";
	}

	OBSThemeVariable val1, val2;
	try {
		val1 = ParseCalcVariable(vars, args[0], recursion);
		val2 = ParseCalcVariable(vars, args[2], recursion);
	} catch (...) {
		return "'Invalid expression'";
	}

	/* Ensure that suffixes match (if any) */
	if (!val1.suffix.isEmpty() && !val2.suffix.isEmpty() &&
	    val1.suffix != val2.suffix) {
		blog(LOG_ERROR,
		     "calc() requires suffixes to match or only one to be present! %s != %s",
		     QT_TO_UTF8(val1.suffix), QT_TO_UTF8(val2.suffix));
		return "'Invalid expression'";
	}

	double val = numeric_limits<double>::quiet_NaN();
	double d1 = val1.userValue.isValid() ? val1.userValue.toDouble()
					     : val1.value.toDouble();
	double d2 = val2.userValue.isValid() ? val2.userValue.toDouble()
					     : val2.value.toDouble();

	if (!isnormal(d1) || !isnormal(d2)) {
		blog(LOG_ERROR,
		     "calc() received at least one invalid value:"
		     " op1: %f, op2: %f",
		     d1, d2);
		return "'Invalid expression'";
	}

	if (opt == "+")
		val = d1 + d2;
	else if (opt == "-")
		val = d1 - d2;
	else if (opt == "*")
		val = d1 * d2;
	else if (opt == "/")
		val = d1 / d2;

	if (!isnormal(val)) {
		blog(LOG_ERROR,
		     "Invalid calc() math resulted in non-normal number:"
		     " %f %s %f = %f",
		     d1, QT_TO_UTF8(opt), d2, val);
		return "'Invalid expression'";
	}

	bool isInteger = ceill(val) == val;
	QString result = QString::number(val, 'f', isInteger ? 0 : -1);

	/* Carry-over suffix */
	if (!val1.suffix.isEmpty())
		result += val1.suffix;
	else if (!val2.suffix.isEmpty())
		result += val2.suffix;

	return result;
}

static QString PresetValue(const QHash<QString, OBSThemeVariable> &vars,
			   const OBSThemeVariable &var)
{
}

static QString PrepareQSS(const QHash<QString, OBSThemeVariable> &vars,
			  const QStringList &contents)
{
	QString stylesheet;
	QString needleTemplate("${%1}");

	for (const QString &content : contents) {
		qsizetype offset = FindEndOfOBSMetadata(content);
		if (offset >= 0) {
			stylesheet += "\n";
			stylesheet += content.sliced(offset);
		}
	}

	for (const OBSThemeVariable &var_ : vars) {
		OBSThemeVariable var(var_);

		if (!ResolveVariable(vars, var))
			continue;

		QString needle = needleTemplate.arg(var_.name);
		QString replace;

		QVariant value = var.userValue.isValid() ? var.userValue
							 : var.value;

		if (var.type == OBSThemeVariable::Color) {
			replace = value.value<QColor>().name(QColor::HexRgb);
		} else if (var.type == OBSThemeVariable::Calc) {
			replace = EvalCalc(vars, var);
		} else if (var.type == OBSThemeVariable::Size ||
			   var.type == OBSThemeVariable::Number ||
			   var.type == OBSThemeVariable::Range) {
			double val = value.toDouble();
			bool isInteger = ceill(val) == val;
			replace = QString::number(val, 'f', isInteger ? 0 : -1);

			if (!var.suffix.isEmpty())
				replace += var.suffix;
		} else {
			replace = value.toString();
		}

		stylesheet = stylesheet.replace(needle, replace);
	}

	return stylesheet;
}

template<typename T> static void FillEnumMap(QHash<QString, T> &map)
{
	QMetaEnum meta = QMetaEnum::fromType<T>();

	int numKeys = meta.keyCount();
	for (int i = 0; i < numKeys; i++) {
		const char *key = meta.key(i);
		QString keyName(key);
		map[keyName.toLower()] = static_cast<T>(meta.keyToValue(key));
	}
}

static QPalette PreparePalette(const QHash<QString, OBSThemeVariable> &vars,
			       const QPalette &defaultPalette)
{
	static QHash<QString, QPalette::ColorRole> roleMap;
	static QHash<QString, QPalette::ColorGroup> groupMap;

	if (roleMap.empty())
		FillEnumMap<QPalette::ColorRole>(roleMap);
	if (groupMap.empty())
		FillEnumMap<QPalette::ColorGroup>(groupMap);

	QPalette pal(defaultPalette);

	for (const OBSThemeVariable &var_ : vars) {
		if (!var_.name.startsWith("palette_"))
			continue;
		if (var_.name.count("_") < 1 || var_.name.count("_") > 2)
			continue;

		OBSThemeVariable var(var_);
		if (!ResolveVariable(vars, var) ||
		    var.type != OBSThemeVariable::Color)
			continue;

		/* Determine role and optionally group based on name.
		 * Format is: palette_<role>[_<group>] */
		QPalette::ColorRole role = QPalette::NoRole;
		QPalette::ColorGroup group = QPalette::All;

		QStringList parts = var_.name.split("_");
		if (parts.length() >= 2) {
			QString key = parts[1].toLower();
			if (!roleMap.contains(key)) {
				blog(LOG_WARNING,
				     "Palette role \"%s\" is not valid!",
				     QT_TO_UTF8(parts[1]));
				continue;
			}
			role = roleMap[key];
		}

		if (parts.length() == 3) {
			QString key = parts[2].toLower();
			if (!groupMap.contains(key)) {
				blog(LOG_WARNING,
				     "Palette group \"%s\" is not valid!",
				     QT_TO_UTF8(parts[2]));
				continue;
			}
			group = groupMap[key];
		}

		QVariant value = var.userValue.isValid() ? var.userValue
							 : var.value;

		QColor color = value.value<QColor>().name(QColor::HexRgb);
		pal.setColor(group, role, color);
	}

	return pal;
}

OBSTheme *OBSApp::GetTheme(const QString &name)
{
	if (!themes.contains(name))
		return nullptr;

	return &themes[name];
}

void OBSApp::themeFileChanged(const QString &path)
{
	themeWatcher->blockSignals(true);
	blog(LOG_INFO, "Theme file \"%s\" changed, reloading...",
	     QT_TO_UTF8(path));
	SetTheme(currentTheme->id);
}

QHash<QString, OBSThemeVariable>
OBSApp::GetThemeVariables(const OBSTheme &theme)
{
	QHash<QString, OBSThemeVariable> vars;
	QStringList themeIds(theme.dependencies);
	themeIds << theme.id;

	for (const QString &themeId : themeIds) {
		blog(LOG_DEBUG, "Processing %s", QT_TO_UTF8(themeId));
		OBSTheme *cur = GetTheme(themeId);

		QFile file(cur->location);
		if (!file.open(QIODeviceBase::ReadOnly))
			return {};

		const QByteArray content = file.readAll();

		for (OBSThemeVariable &var :
		     ParseThemeVariables(content.constData())) {
			vars[var.name] = std::move(var);
		}
	}

	return vars;
}

#define DEBUG_THEME_OUTPUT

bool OBSApp::SetTheme(const QString &name)
{
	OBSTheme *theme = GetTheme(name);
	if (!theme)
		return false;

	setStyleSheet("");
	currentTheme = theme;

	userVariables.clear();
	themeWatcher->removePaths(themeWatcher->files());

	QStringList contents;
	QHash<QString, OBSThemeVariable> vars;
	/* Build list of themes to load (in order) */
	QStringList themeIds(theme->dependencies);
	themeIds << theme->id;

	/* Find and add high contrast adjustment layer if available */
	if (HighContrastEnabled()) {
		for (const OBSTheme &theme_ : themes) {
			if (!theme_.isHighContrast)
				continue;
			if (theme_.parent != theme->id)
				continue;
			themeIds << theme_.id;
			break;
		}
	}

	QStringList filenames;
	for (const QString &themeId : themeIds) {
		OBSTheme *cur = GetTheme(themeId);

		QFile file(cur->location);
		filenames << cur->location;

		if (!file.open(QIODeviceBase::ReadOnly))
			return false;
		const QByteArray content = file.readAll();

		for (OBSThemeVariable &var :
		     ParseThemeVariables(content.constData())) {
			if (var.editable)
				userVariables[var.name] = var;

			vars[var.name] = std::move(var);
		}

		contents.emplaceBack(content.constData());
	}

	ParseThemeCustomisation(vars);
	const QString stylesheet = PrepareQSS(vars, contents);
	const QPalette palette = PreparePalette(vars, defaultPalette);
	setPalette(palette);
	setStyleSheet(stylesheet);

#ifdef DEBUG_THEME_OUTPUT
	/* Debug shit remove later */
#ifdef __APPLE__
	string filename("obs-studio/themes/");
	filename += theme->id.toStdString();
	filename += ".out";

	filesystem::path debugOut;
	char configPath[512];
	if (GetConfigPath(configPath, sizeof(configPath), filename.c_str())) {
		debugOut = absolute(filesystem::u8path(configPath));
		filesystem::create_directories(debugOut.parent_path());
	}
#else
	QString debugOut = theme->location + ".out";
#endif
	QFile debugFile(debugOut);
	if (debugFile.open(QIODeviceBase::WriteOnly)) {
		debugFile.write(stylesheet.toUtf8());
		debugFile.flush();
	}
#endif

#ifdef __APPLE__
	SetMacOSDarkMode(theme->isDark);
#endif

	emit StyleChanged();
	themeWatcher->addPaths(filenames);
	QTimer::singleShot(1000, this,
			   [&] { themeWatcher->blockSignals(false); });

	return true;
}

static map<string, string> themeMigrations = {
	{"Yami", DEFAULT_THEME},
	// {"Dark", "com.obsproject.Classic"}, /* Doesn't really work yet */
	{"Grey", "com.obsproject.Yami.Grey"},
};

bool OBSApp::InitTheme()
{
	defaultPalette = palette();
	setStyle(new OBSProxyStyle());

	/* Set search paths for custom 'theme:' URI prefix */
	string searchDir;
	if (GetDataFilePath("themes", searchDir)) {
		auto installSearchDir = filesystem::u8path(searchDir);
		QDir::addSearchPath("theme", absolute(installSearchDir));
	}

	char userDir[512];
	if (GetConfigPath(userDir, sizeof(userDir), "obs-studio/themes")) {
		auto configSearchDir = filesystem::u8path(userDir);
		QDir::addSearchPath("theme", absolute(configSearchDir));
	}

	/* Load list of themes and read their metadata */
	FindThemes();

	/* Set up Qt file watcher to automatically reload themes */
	themeWatcher = new QFileSystemWatcher(this);
	connect(themeWatcher.get(), &QFileSystemWatcher::fileChanged, this,
		&OBSApp::themeFileChanged);

	/* Migrate old theme config key */
	if (config_has_user_value(globalConfig, "General", "CurrentTheme3") &&
	    !config_has_user_value(globalConfig, "Appearance", "Theme")) {
		const char *old = config_get_string(globalConfig, "General",
						    "CurrentTheme3");

		if (themeMigrations.count(old)) {
			config_set_string(globalConfig, "Appearance", "Theme",
					  themeMigrations[old].c_str());
		}
	}

	QString themeName =
		config_get_string(globalConfig, "Appearance", "Theme");

	if (themeName.isEmpty() || !GetTheme(themeName))
		themeName = DEFAULT_THEME;

	return SetTheme(themeName);
}
