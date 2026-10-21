# Independent review

Reviewer: separate read-only agent `/root/review_issue8`, uncommitted diff against e702d34.

1. MAJOR — doctor used `test -e/-L` and could treat an inaccessible new parent as absent, selecting a valid legacy key despite driver EACCES. Fixed by checking ancestor directory traversal before fallback. Added inaccessible-parent and ENOTDIR cases; both failed before correction and passed afterward (23/23 total).
2. MINOR — English/Turkish MAC-failure troubleshooting backed up only the legacy directory before replacement. Fixed to back up the PSK path reported by doctor, with an explicit variable setup instruction.

No BLOCKER/CRITICAL reported. Findings addressed by implementer and regression evidence; reviewer did not rerun after fixes. Human approval and Fedora enforcing acceptance remain pending.
