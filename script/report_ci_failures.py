#!/usr/bin/env python3
"""Maintain one issue for unresolved build, release and update workflow failures."""

import json
import os
from urllib.parse import quote
from urllib.request import Request, urlopen

TITLE = "CI: unresolved build and release failures"
BAD_RESULTS = {"failure", "timed_out", "startup_failure", "action_required"}
STATE_MARKER = "<!-- yacht-ci-state:"


def failed_jobs(jobs):
    return [job for job in jobs if job.get("conclusion") in BAD_RESULTS]


def read_state(issue):
    body = (issue or {}).get("body") or ""
    start = body.find(STATE_MARKER)
    if start < 0:
        return {}
    end = body.find(" -->", start)
    if end < 0:
        return {}
    try:
        state = json.loads(body[start + len(STATE_MARKER):end])
    except json.JSONDecodeError:
        return {}
    return state if isinstance(state, dict) else {}


def issue_body(state):
    lines = [
        "Unresolved build, release or update failures. Development may continue while these are investigated.",
        "",
    ]
    for key, record in sorted(state.items()):
        lines += [f"### {key}", "", f"Run: {record['run']}", f"Commit: `{record['sha']}`", "", "| Job | Result |", "| --- | --- |"]
        for job in record["jobs"]:
            name = job["name"].replace("|", "\\|")
            lines.append(f"| [{name}]({job['url']}) | {job['result']} |")
        lines.append("")
    lines += ["Fix each affected platform or check before describing its artifacts as verified.", "", STATE_MARKER + json.dumps(state, separators=(",", ":")) + " -->"]
    return "\n".join(lines)


def api(method, path, payload=None):
    root = os.environ.get("GITHUB_API_URL", "https://api.github.com").rstrip("/")
    token = os.environ["GH_TOKEN"]
    data = json.dumps(payload).encode() if payload is not None else None
    request = Request(
        root + path,
        data=data,
        method=method,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    with urlopen(request, timeout=30) as response:
        return json.load(response) if response.status != 204 else None


def main():
    repo = os.environ["GITHUB_REPOSITORY"]
    run_id = os.environ["GITHUB_RUN_ID"]
    attempt = os.environ["GITHUB_RUN_ATTEMPT"]
    sha = os.environ["GITHUB_SHA"]
    ref = os.environ.get("GITHUB_REF", "refs/heads/main")
    key = os.environ.get("CI_REPORT_KEY") or ("main" if ref == "refs/heads/main" else "tag:" + ref.removeprefix("refs/tags/"))
    base = "/repos/" + "/".join(quote(part, safe="") for part in repo.split("/"))
    jobs = api("GET", f"{base}/actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100")["jobs"]
    failures = failed_jobs(jobs)
    issues = api("GET", f"{base}/issues?state=all&per_page=100")
    existing = next((issue for issue in issues if "pull_request" not in issue and issue["title"] == TITLE), None)
    state = read_state(existing)

    if failures:
        state[key] = {
            "run": f"https://github.com/{repo}/actions/runs/{run_id}/attempts/{attempt}",
            "sha": sha,
            "jobs": [{"name": job["name"], "url": job["html_url"], "result": job["conclusion"]} for job in failures],
        }
    else:
        state.pop(key, None)

    if not state:
        if existing and existing["state"] == "open":
            api("PATCH", f"{base}/issues/{existing['number']}", {"state": "closed", "state_reason": "completed"})
            print(f"Closed resolved CI issue #{existing['number']}")
        else:
            print("No unresolved CI failures")
        return

    body = issue_body(state)
    if existing:
        api("PATCH", f"{base}/issues/{existing['number']}", {"state": "open", "body": body})
        print(f"Updated CI issue #{existing['number']}")
    else:
        issue = api("POST", f"{base}/issues", {"title": TITLE, "body": body})
        print(f"Created CI issue #{issue['number']}")


if __name__ == "__main__":
    main()
