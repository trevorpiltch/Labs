# Labs
## About
I've fallen into the trap of reading books and watching videos while trying to learn something. Taking the idea from science, 
this repo contains labs for gaining hands-on experience writing software. It contains projects, scripts, and experiments
that I use to learn and explore new languages and frameworks.

## Format
Each lab is a directory in this repo containing a README that explains what the lab is and why I decided to do it.
Labs are self-contained: they bring their own build tooling and their own `.gitignore` for build output, and nothing
outside a lab's directory depends on it.

A lab README has three sections:

- **ABOUT** — what the lab is, and the book, course, or exercise set it follows.
- **MOTIVATION** — why I picked it up.
- **BUILDING** — what you need installed and how to run it.

## Labs
| Lab | What it is |
| --- | --- |
| [arm-assembly](arm-assembly) | ARM assembly from *Programming with 64-Bit ARM Assembly Language*, built for a Raspberry Pi 5 |
| [Depot-Tutorial](Depot-Tutorial) | The Depot store from *Agile Web Development with Rails 6* — Ruby, Rails, and MVC on the web |
| [go](go) | Command-line tools from *Powerful Command-Line Applications in Go* |
| [rustlings](rustlings) | The rustlings exercise set — ownership, borrowing, and lifetimes by arguing with the borrow checker |

## Adding a lab
Drop the directory in and commit it. No new repo, no submodule:

```
cp -r ~/Developer/projects/<name> ~/Developer/resources/labs/<name>
rm -rf <name>/.git                  # if it was a repo of its own
```

Then give it a README in the format above, make sure its build output is covered by a `.gitignore`
inside the lab directory, and commit.

If the lab already exists as a GitHub repo whose history is worth keeping, pull it in with its
commits instead of copying the files:

```
git subtree add --prefix=<name> <clone-url> main
```

## History
Labs used to be git submodules, each with its own repo. They were flattened into this repo in
September 2026; the submodule history is preserved through the subtree merges, so a lab's original
commits are still reachable — reach them through the merge that imported them, e.g.
`git log $(git log --format=%H --grep="Add '<name>/' from commit" -1)^2`. The tag `pre-monorepo`
marks the last commit before the conversion.
