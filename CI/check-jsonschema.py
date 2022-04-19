import json
from jsonschema import Draft7Validator
from json_source_map import calculate
import sys

if len(sys.argv) < 2:
    print("JSON path required.")

if len(sys.argv) < 3:
    print("JSON schema path required.")
    quit()

services = {}
servicesRaw = ''
schema = {}

with open(sys.argv[1]) as json_file:
    servicesRaw = json_file.read()
    services = json.loads(servicesRaw)

with open(sys.argv[2]) as json_file:
    schema = json.load(json_file)

servicesPaths = calculate(servicesRaw)
errors = []

cls = Draft7Validator(schema)

for e in sorted(cls.iter_errors(services), key=str):
    print(e)
    errorPath = '/'.join(str(v) for v in e.absolute_path)
    errorEntry = servicesPaths['/' + errorPath]
    errors.append({
        "file": sys.argv[1],
        "start_line": errorEntry.value_start.line + 1,
        "end_line": errorEntry.value_end.line + 1,
        "title": "Validation Error",
        "message": e.message,
        "annotation_level": "failure"
    })

if len(errors) > 0:
    with open('validation_errors.json', 'w') as outfile:
        json.dump(errors, outfile)
    sys.exit(1)
