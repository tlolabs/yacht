#!/usr/bin/env python3
"""Maintain one issue for unresolved failures on the latest main workflow run."""

import json
import os
from urllib.parse import quote
from urllib.request import Request, urlopen

TITLE = "CI: unresolved main-branch failures"
BAD_RESULTS = {"failure", "timed_out", "startup_failure", "action_required"}


def failed_jobs(jobs):
    return [job for job in jobs if job.get("conclusion") in BAD_RESULTS]


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
    base = "/repos/" + "/".join(quote(part, safe="") for part in repo.split("/"))
    jobs = api("GET", f"{base}/actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100")["jobs"]
    failures = failed_jobs(jobs)
    issues = api("GET", f"{base}/issues?state=all&per_page=100")
    existing = next((issue for issue in issues if "pull_request" not in issue and issue["title"] == TITLE), None)

    if not failures:
        if existing and existing["state"] == "open":
            api("PATCH", f"{base}/issues/{existing['number']}", {"state": "closed", "state_reason": "completed"})
            print(f"Closed resolved CI issue #{existing['number']}")
        else:
            print("No unresolved CI failures")
        return

    lines = [
        "Failures in the latest `main` workflow run. Development may continue while these are investigated.",
        "",
        f"Run: https://github.com/{repo}/actions/runs/{run_id}/attempts/{attempt}",
        f"Commit: `{sha}`",
        "",
        "| Job | Result |",
        "| --- | --- |",
    ]
    for job in failures:
        name = job["name"].replace("|", "\\|")
        lines.append(f"| [{name}]({job['html_url']}) | {job['conclusion']} |")
    lines += ["", "Fix each affected platform or check before describing its artifacts as verified."]
    body = "\n".join(lines)
    if existing:
        api("PATCH", f"{base}/issues/{existing['number']}", {"state": "open", "body": body})
        print(f"Updated CI issue #{existing['number']}")
    else:
        issue = api("POST", f"{base}/issues", {"title": TITLE, "body": body})
        print(f"Created CI issue #{issue['number']}")


if __name__ == "__main__":
    main()
