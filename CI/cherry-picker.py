import os
import sys

from subprocess import check_output, STDOUT, CalledProcessError

import requests

CHERRY_PICK_TAG_PREFIX = 'cherry-pick-to-'
PR_MESSAGE = '''The following issue occured when trying to cherry pick this PR to the target branch(es):
{}

Please merge the changes manually.'''


def get_pr_targets(s, repo, pr_num):
    """
    Fetch labels from PR, get cherry-pick tags and return target branches

    :return: list of target branch names
    """

    # fetch run first, get workflow id from there to get workflow runs
    r = s.get(f'https://api.github.com/repos/{repo}/pulls/{pr_num}')
    r.raise_for_status()
    j = r.json()

    targets = []
    for label in j['labels']:
        name = label['name']
        if not name.startswith(CHERRY_PICK_TAG_PREFIX):
            continue
        targets.append(name.replace(CHERRY_PICK_TAG_PREFIX, ''))

    return targets


def get_pr_commits(s, repo, pr_num):
    """
    Fetch commits for

    :return: list of target branch names
    """

    # fetch run first, get workflow id from there to get workflow runs
    r = s.get(f'https://api.github.com/repos/{repo}/pulls/{pr_num}/commits')
    r.raise_for_status()
    j = r.json()

    return [c['sha'] for c in j]


def post_pr_comment(s, repo, pr_num, warnings):
    formatted_warnings = '\n'.join(f'- {e.decode("utf-8")}' for e in warnings)
    comment = PR_MESSAGE.format(formatted_warnings)

    r = s.post(f'https://api.github.com/repos/{repo}/issues/{pr_num}/comments',
               json=dict(body=comment))
    r.raise_for_status()


def cherry_pick_to(target, commits):
    # switch to target branch
    try:
        print(f'Checking out branch "{target}"...')
        _ = check_output(['git', 'checkout', target], stderr=STDOUT)
    except CalledProcessError as e:
        print('Checking out failed!')
        return e.output

    try:
        print(f'Picking commits to "{target}"...')
        _ = check_output(['git', 'cherry-pick', *commits], stderr=STDOUT)
        return ''
    except CalledProcessError as e:
        print('Cherry-picking failed!')
        return e.output


def push_branches(branches):
    # switch to target branch
    try:
        _ = check_output(['git', 'push', 'origin', *branches], stderr=STDOUT)
    except CalledProcessError as e:
        return e.output


def main():
    session = requests.session()
    session.headers['Accept'] = 'application/vnd.github+json'
    session.headers['Authorization'] = f'Bearer {os.environ["GITHUB_TOKEN"]}'
    repo = os.environ['REPOSITORY']
    pr_number = os.environ['PR_NUMBER']

    targets = get_pr_targets(session, repo, pr_number)
    if not targets:
        print(f'No cherry pick requested for PR {pr_number}')
        return 0

    warnings = []
    # get the PR's commits to pick
    commits = get_pr_commits(session, repo, pr_number)
    if not commits:
        print('WTF?!')
        return 1

    branches = []
    for target in targets:
        if res := cherry_pick_to(target, commits):
            warnings.append(res)
        else:
            branches.append(target)

    push_err = push_branches(branches)
    if push_err:
        warnings.append(push_err)

    if warnings:
        post_pr_comment(session, repo, pr_number, warnings)

    # if no branches were successfully picked or push failed, error out and quit
    if not branches or push_err:
        return 1

    return 0


if __name__ == '__main__':
    sys.exit(main())
