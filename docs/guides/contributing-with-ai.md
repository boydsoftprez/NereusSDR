# Contribute to NereusSDR with AI

Bring your curiosity. NereusSDR is a community project spanning an OpenHPSDR
station engine, a desktop console, native iPhone and iPad clients, and the
documentation that connects them. You can help improve any of it, and learn
how to work in a real repository along the way.

Strong coding skills are not a prerequisite. Bring curiosity and a
willingness to learn something new. Know what you want to improve, describe
it accurately, and test whether the result does what you intended.

A clearer instruction, a reproducible bug report, a focused test or a
small code improvement can all move the project forward. Experienced
developers and people making their first pull request are welcome.
Come [meet the community on Discord](https://discord.gg/m35ERjwRe)
and browse the [open issues](https://github.com/boydsoftprez/NereusSDR/issues).

## Report an issue

A clear issue report is a contribution too. Describe what you wanted to
do, what happened, and how someone else can reproduce it.

1. **Click the lightbulb** in the NereusSDR desktop console to open the
   **AI-Assisted Issue Reporter**.
2. **Follow the reporter's directions.** If you choose an AI assistant,
   its button copies a prompt to your clipboard. Paste it into the AI chat,
   replace the bracketed description with your problem, and review the
   resulting report for accuracy.
3. **Choose Report a Bug** to open the GitHub bug-report form. Sign in to
   GitHub if prompted. Copy your reviewed description into the form and
   complete the requested details: steps to reproduce, radio model and
   protocol, NereusSDR version, operating system, and relevant firmware
   information. Attach useful logs or screenshots when available.
4. **Review the completed form and submit the issue on GitHub.** Include
   what you expected to happen as well as what actually happened, so
   others can investigate and test a fix.

You can also open the
[GitHub bug reporter directly](https://github.com/boydsoftprez/NereusSDR/issues/new?template=bug_report.yml).
For a new idea, choose **Submit Your Idea** in the lightbulb dialog or use
the [feature-request form](https://github.com/boydsoftprez/NereusSDR/issues/new?template=feature_request.yml).

## Choose your tools

Use [Codex](https://openai.com/codex/),
[Claude Code](https://code.claude.com/docs/en/overview),
[OpenCode](https://opencode.ai/docs/) or another coding harness you enjoy.
For sustained work, choose a harness that can read repository files, edit
them, run commands and use the results to decide its next step. The harness
provides those tools; the model helps reason about what to do with them.

Give it a clear goal and the repository instructions. Ask it to investigate,
make a plan, carry out a focused change and check the result. For longer
tasks, keep the agreed plan and verification results in working notes so
you can resume with the same context. You remain the contributor: read the
diff, understand the change and review what you submit.

### Try OpenCode and Space Bunny Free

Want to experiment with a free model? As of 6 October 2026, OpenCode Zen
lists **Space Bunny Free** (`space-bunny-free`) as a **limited-time free
model**. Availability and pricing can change; check the current
[Zen model list](https://opencode.ai/docs/en/zen/) before starting.

With Node.js and npm installed, one supported installation method is:

```bash
npm install -g opencode-ai
```

OpenCode's [installation guide](https://opencode.ai/docs/) has other options.
Once you have cloned your fork using the steps below, launch it inside the
repository:

```bash
opencode
```

Use `/connect` to configure the OpenCode provider if needed, following its
current account and API-key setup. Use `/models` to select **Space Bunny
Free** when available. Confirm the selected model's price; other models
may be paid. The [model selector documentation](https://opencode.ai/docs/models/)
explains switching models. Then paste the orientation prompt below.

## Fork the project and make a branch

Install Git and create a GitHub account if you have not already. Visit
[boydsoftprez/NereusSDR](https://github.com/boydsoftprez/NereusSDR) and click
**Fork** to create a copy under your own account.

In a terminal, replace `YOUR-GITHUB-USERNAME` with your GitHub username:

```bash
git clone https://github.com/YOUR-GITHUB-USERNAME/NereusSDR.git
cd NereusSDR
git remote add upstream https://github.com/boydsoftprez/NereusSDR.git
git fetch upstream
git switch -c contrib/first-change upstream/main
bash scripts/install-hooks.sh
```

`origin` is your fork, where you will push your work. `upstream` is the
shared NereusSDR repository. Your new branch starts from its current
`main`; keep one focused change on that branch. On Windows, use Git Bash
for these shell commands. GitHub's
[fork documentation](https://docs.github.com/en/pull-requests/how-tos/work-with-forks)
explains the relationship between the two repositories.

Open this cloned folder in your coding harness. Read `AGENTS.md` if present,
then [CLAUDE.md](../../CLAUDE.md), [CONTRIBUTING.md](../../CONTRIBUTING.md)
and the relevant project docs. The file named `CLAUDE.md` contains useful
project instructions whichever harness you choose. Ask your harness to
read these existing instructions before it starts changing files.

## Start with something small

A good first task is to clarify an instruction you found confusing in the
[user guide](../manual/README.md). Explain where you got stuck and what
would have helped. Ask the harness to check the relevant source and improve
that paragraph without making promises the software cannot keep.

For a code change, pick a reproducible issue and keep the first PR narrow.
Discuss new features, visual or UX changes, architecture, user-facing
defaults and DSP changes with the maintainer before implementing them.
NereusSDR's remote Core, slices and panadapters have their own architecture;
an implementation needs to fit that structure.

If you use upstream code, read
[HOW-TO-PORT.md](../attribution/HOW-TO-PORT.md) first. Preserve its required
copyright headers, inline comments and provenance, and cite the actual
source for protocol, hardware and DSP facts. Record the human author and
AI tooling in the required modification history. These are part of the
contribution, not a cleanup task to leave for someone else.

## Prompts to paste into your harness

Replace the bracketed text with your own task. Run the orientation prompt
first, then choose a change prompt. Ask follow-up questions when something
in the answer does not make sense: learning the repo is part of the work.

### 1. Get oriented

```text
I want to make my first contribution to NereusSDR and learn as I go.
My goal is [describe what I want to improve and what a good result would
look like]. Help me clarify that goal and how I can test the result.
Read AGENTS.md if it
exists, CLAUDE.md, CONTRIBUTING.md, README.md, docs/architecture/overview.md
and docs/development/fast-test-loop.md. Inspect the current branch and
working tree without changing anything.

Explain how the Core, desktop console and phone client fit together.
Suggest three small contributions I can learn from, including one docs
task. For each, identify the relevant files and how
we would verify it. Do not edit yet; help me choose a focused task.
```

### 2. Make a first documentation change

```text
Help me improve this confusing instruction: [paste the paragraph and
describe where you got stuck]. Find its source under docs/ and check
the relevant implementation or existing documentation.

Propose a small edit that preserves the software's actual behavior.
Explain the evidence for it, then make the agreed edit. If this page is
published on the website, follow website/README.md to regenerate it.
Run the relevant docs checks, review the diff and report the exact checks
you ran. Do not invent features, test results or hardware validation.
```

### 3. Investigate and fix a bug

```text
Investigate this issue: [issue URL, reproduction steps and environment].
Read the repository instructions and relevant source first. Reproduce
the problem if possible, explain the root cause and propose a focused
fix with a meaningful regression check.

Follow NereusSDR's AppSettings and thread rules. Keep GUI dependencies
out of src/core and src/models. Use cited sources for protocol, hardware
and WDSP facts; read HOW-TO-PORT.md before porting code. Flag changes to
the core RX path. Ask me to discuss any required UX, architecture,
feature-scope, default or DSP decision with the maintainer first.

Implement the agreed fix, build the matching test target before running
its tests, and report actual results and what remains untested. Keep
unrelated changes out of the diff.
```

### 4. Review and prepare the PR

```text
Review our complete diff against upstream/main and the repository rules.
Look for incorrect claims, unrelated changes, missing attribution and
tests that do not exercise the problem. Check git diff --check.

Summarize what changed and why in plain language. List the exact checks
run and their results, plus platforms or radio behavior we did not test.
Draft a PR description using .github/PULL_REQUEST_TEMPLATE.md, linking
the issue if there is one. State how AI helped and what I should review
before submitting. Do not commit or push until I have read the diff.
```

## Verify the change

For application code, install the platform dependencies listed in
[Building from Source](https://github.com/boydsoftprez/NereusSDR#building-from-source)
and [CONTRIBUTING.md](../../CONTRIBUTING.md). The project uses C++20 and Qt6.
The usual configure and application build are:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON
cmake --build build --parallel
```

`NEREUS_BUILD_TESTS=ON` makes the test targets available; building them
remains opt-in. Read [the fast test loop](../development/fast-test-loop.md) before testing.
Build the specific test target that covers your change before running it;
a normal application build does not rebuild the test executables. Start
with the relevant test, then follow the project's broader verification
requirements for the affected code. Tell reviewers which platform and
radio you used and which behavior you could not check.

For user-guide or website-guide changes, follow
[website/README.md](../../website/README.md) to install the renderer's
dependencies, then run:

```bash
python3 docs/manual/check_manual.py
python3 docs/manual/build_preview.py --website
python3 -m unittest discover -s docs/manual -p 'test_build_preview.py'
```

Review the generated pages as well as the Markdown, and include the
generated output when required by the website workflow. Before committing
any change, inspect it yourself:

```bash
git status --short
git diff --check
git diff
```

## Commit, push and open a pull request

Set Git's author name and email to your own identity. This repository
requires GPG-signed commits for integration into `main`; follow the
[commit-signing instructions](../../CONTRIBUTING.md#commit-signing) to
configure your key. Stage the specific files you reviewed with
`git add PATH-TO-FILE`, including any required generated pages.

```bash
git commit -S -m "docs: clarify the connection instructions"
git push -u origin contrib/first-change
```

Change the commit message to describe your actual contribution. `-S`
requests the GPG signature. If signing fails, fix the signing setup
before pushing the contribution.

On GitHub, open your fork and click **Compare & pull request**. Check
that the **base repository** is `boydsoftprez/NereusSDR`, the **base branch**
is `main`, and the **head branch** is your fork's `contrib/first-change`.
Use **compare across forks** if needed. GitHub documents this in
[creating a PR from a fork](https://docs.github.com/en/pull-requests/how-tos/create-pull-requests/creating-a-pull-request-from-a-fork).

Fill in the PR template: explain the problem and change, list the checks
you actually ran, and include attribution when porting code. Link the
relevant issue; use `Fixes #NUMBER` only when the PR resolves that issue.
Mention how AI assisted your work. A draft PR is welcome when you want
feedback while learning or have verification still to do.

The maintainer and community review the proposal and CI checks it. Make
requested edits on the same branch, commit and push again; they appear
in the same PR. Once accepted and merged, your contribution becomes part
of the shared project and can reach everyone in a future release.

You can contribute from wherever you are, with whatever capable harness
helps you work. Bring a question, a useful improvement and a willingness
to learn together.
