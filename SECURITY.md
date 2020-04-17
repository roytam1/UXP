# Security Policy

## What is or is not a vulnerability?

In general, vulnerabilities are those bugs that can actually be exploited to perform malicious tasks. 
Most _crashes_ are not security vulnerabilities. Although important to fix, they don't inherently cause a problem for the 
browser's security.

What should be considered vulnerabilities or security hazards by default:
- Use-after-free crashes, since those can potentially be used for remote code execution;
- Spoofing issues in the UI;
- File security issues, like out-of-bounds access to arbitrary files or locations;
- Type confusion issues;
- Bypass of security measures like CSP or the various mechanisms around HTTPS.

Generally not security vulnerabilities:
- Null dereferencing crashes;
- Malware extensions (but please do report those on the forum in the extensions board!);
- Denial-of-service (AKA "evil trap sites")
- Browser hangs
- Issues with non-standard manual configuration (either at build time or by manipulating about:config directly)

## Reporting a Vulnerability

If you find an issue in UXP or the applications it builds on that could impact the security or safety of users please **do not**
make an issue on GitHub about it. GitHub does not support restricted viewability for security sensitive bugs.

If you want to report a security-sensitive issue then please go to the [forum](https://forum.palemoon.org) and report the issue
via a **private message** to **Moonchild** (the founder and prime responsible for security issues).
The forum's private message system is fully secure since your visits are encrypted and private messages are not available to anyone
except the recipient (not even moderators!).

You will be informed via private message if the vulnerability report is accepted or declined, with reasoning.
Security updates occur regularly and are given priority over most other development tasks. In general, they can be solved
relatively quickly and will be included in the next point release (third digit if not rolled into a more major one).

