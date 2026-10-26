# Security and persistence notes

## Password storage (NFR-03) - libsodium Argon2id
Passwords are hashed with **libsodium** `crypto_pwhash_str` using **Argon2id**
(`PasswordHasher`).
* *Recognised and maintained:* libsodium is a widely audited library; Argon2id won the Password
  Hashing Competition and is the algorithm recommended first by OWASP.
* *Salted:* a fresh random 16-byte salt per password; salt and parameters are inside the stored
  string (`$argon2id$v=19$m=...,t=...,p=...$salt$hash`), so no separate salt column is needed and
  identical passwords give different hashes.
* *Memory-hard:* resists GPU/ASIC guessing better than SHA-2 or bcrypt. The "interactive" setting
  (2 passes, 64 MiB) takes ~50-100 ms on a modern PC; measured login with 800+ users: ~55 ms
  (limit 2 s, NFR-01).
* *Upgradeable:* `needsRehash` re-hashes at login if settings are strengthened later.
* *Constant-time verify*, and a dummy verification for unknown usernames so response time does
  not reveal which usernames exist.
Rejected alternatives: plain SHA-256 (fast, so guessable), hand-written PBKDF2 (own crypto code).

## Other controls
* Role-based access control in the service layer (`AccessControl`, `User::permissions()`), plus
  ownership checks (lecturer owns course, HoD owns department, student owns record). Records of
  other users answer "not found", not "forbidden", so ids cannot be probed.
* Generic login error, 5-failure temporary lock (5 minutes), deactivated accounts cannot log in and
  an active session ends at the next screen after deactivation.
* Parameterised SQL only; schema CHECKs, foreign keys (enabled on every connection), UNIQUE
  constraints; triggers that refuse marks above the maximum, release without moderation approval,
  submissions by non-enrolled students, and any change to the audit log.
* Mark release is enforced twice: `ModerationService` (application) and trigger
  `prevent_unapproved_mark_release` (database). Students' mark queries only select released rows.
* Uploaded files: type allow-list, 10 MB limit, sanitised stored names (no path traversal), stored
  outside the database in `lms_data/`.
* `PRAGMA synchronous = FULL` and transactions around every multi-step change, so a power cut
  cannot leave half a moderation or marking operation.
* Log file records security events, never passwords.

## Known limits (honest scope)
* The database and uploaded files are not encrypted at rest; protect the computer and backups
  physically / with disk encryption.
* Single-computer use. SQLite on a network share is not supported.
