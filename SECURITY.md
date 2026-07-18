# Security Policy

## Scope

This project is an experimental kernel driver and userspace stack for the
Goodix GXFP5130 fingerprint sensor. The scope of security issues includes:

- Privilege escalation via the kernel module or udev rules
- Unsafe handling of the TLS-PSK (key leakage, weak key generation)
- Vulnerabilities in the userspace tools that run as root

Out of scope: security of the sensor hardware itself, Goodix firmware, or the
upstream Void755 components (report those upstream).

## Reporting a vulnerability

Do not open a public GitHub issue for security vulnerabilities.

Email **metehangnen@gmail.com** with:
- A description of the vulnerability
- Steps to reproduce
- Affected component (kernel module / userspace / scripts)
- Your assessment of the impact

You will receive a response within 7 days. Please allow time to prepare a fix
before public disclosure.
