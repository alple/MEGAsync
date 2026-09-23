---
name: grilling
description: Grill the user relentlessly about a plan, decision, or idea. Use when the user wants to stress-test their thinking, or uses any 'grill' trigger phrases.
---

Interview the user relentlessly until you reach a shared understanding. Map this as a **design tree**: every decision branches into the decisions that hang off it.

Work the tree in **rounds**. The **frontier** is every decision whose prerequisites are already settled: the questions you can ask _now_ without guessing at answers you haven't heard yet. Ask the whole frontier in one round: number each question and give your recommended answer. Then wait for the user's answers before the next round.

Format a round like so:

```
❓ **Q1** - **<question title>**: <question body, might be multiple paragraphs, including multiple choices>

➡️ <your recommended answer>

---

❓ **Q2** - **<question title>**: <question body, might be multiple paragraphs, including multiple choices>

➡️ <your recommended answer>
```

Each round the user answers reshapes the tree: settled decisions push the frontier outward and unblock questions that depended on them. Recompute the frontier and ask the next round. A question whose answer depends on another question still open in this round belongs to a _later_ round, not this one.

Finding _facts_ is your job, never the user's. When a frontier question needs a fact from the environment (filesystem, tools, etc.), dispatch a sub-agent to find it; don't ask the user for anything you could look up yourself. Don't block on it: a running exploration is an unsettled prerequisite, so only the questions downstream of it wait for the sub-agent to report; ask the rest of the frontier now. The _decisions_ are the user's: put each to them and wait.

The session is done when the frontier is empty: every branch of the design tree visited, nothing left silently assumed. Do not act on it until the user confirms you have reached a shared understanding.

## Asking questions in the UI

Ask each round's frontier with the `question` tool — not markdown blocks. It renders an interactive prompt in the UI (clickable options, plus a "type your own answer" fallback), instead of `❓` text the user has to reply to in prose.

- One `question` tool call per round; it accepts one or many questions.
- Single question → one entry in the `questions` array.
- Whole frontier → one entry per question, all in a single call. The UI presents them together; the user answers all before the next round.
- Header is the short title (≤30 chars). The full question body (the multiple paragraphs / choices that used to go in the `❓` block) goes in the `question` field.
- Options: concise label (1–5 words) + description for the details. Put the recommended answer first and suffix the label with `(Recommended)`.
- Do NOT add a catch-all "Other" option — the free-text fallback is automatic.
- Multi-choice frontiers: set `multiple: true` per question.

Example — one question:

```
question tool call with questions: [
  { header: "Deploy target", question: "...", options: [ { label: "Vercel (Recommended)", description: "..." }, ... ] }
]
```

Example — full frontier in one round (many questions):

```
same call, questions array with N entries, one per frontier question
```

## Shared-artifact audit (mandatory before placement decisions)

Before proposing or asking the user to lock any decision about *where* something lives (field on a schema vs view, method on entity vs service, etc.), enumerate the artifact's call sites and put the blast radius in the question body:

1. Grep for every usage of the artifact being modified (class, schema, function, endpoint) — yourself, never the user.
2. List each call site in the question: its direction (dump/load, read/write) and its consumer.
3. If more than one call site exists, the placement question MUST show the full list and state per-site consequences of each option. Never present a single-site solution without this list.
4. A rationale for a placement option is incomplete unless it accounts for every call site found — including dormant/echo paths.

Rationale: placement decisions made while looking at one call site silently change the contract for the others. The user can only judge the trad
