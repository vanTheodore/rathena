<img src="doc/logo.png" align="right" height="90" />

# rAthena
![C++23 toolchain](https://github.com/vanTheodore/rathena/actions/workflows/modern-toolchain.yml/badge.svg?branch=master)
![C++23 contract tests](https://github.com/vanTheodore/rathena/actions/workflows/modernization-contracts.yml/badge.svg?branch=master)

![GitHub](https://img.shields.io/github/license/rathena/rathena.svg) 
![commit activity](https://img.shields.io/github/commit-activity/w/rathena/rathena) 
![GitHub repo size](https://img.shields.io/github/repo-size/rathena/rathena.svg)
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/rathena/rathena)


> rAthena is a collaborative software development project revolving around the creation of a robust massively multiplayer online role playing game (MMORPG) server package. Written in C++, the program is very versatile and provides NPCs, warps and modifications. The project is jointly managed by a group of volunteers located around the world as well as a tremendous community providing QA and support. rAthena is a continuation of the eAthena project.

[Forum](https://rathena.org/board)|[Discord](https://rathena.org/discord)|[Wiki](https://github.com/rathena/rathena/wiki)|[FluxCP](https://github.com/rathena/FluxCP)|[Crowdfunding](https://rathena.org/board/crowdfunding/)|[Fork and Pull Request Q&A](https://rathena.org/board/topic/86913-pull-request-qa/)
--------|--------|--------|--------|--------|--------

## This C++23 modernization fork

This repository is an independent modernization of [upstream rAthena](https://github.com/rathena/rathena), with **ISO C++23** as its build baseline. It is not an official upstream release. Source and community links below refer to upstream rAthena.

- **Required build tools:** CMake 3.28+, GCC 14+ / Clang 18+ / Visual Studio 2022 17.8+; C++ extensions disabled by default. See [MODERNIZATION.md](doc/MODERNIZATION.md).
- **Continuous integration:** [multi-platform compilation and sanitizers](https://github.com/vanTheodore/rathena/actions/workflows/modern-toolchain.yml) plus [standalone behavioral contract tests](https://github.com/vanTheodore/rathena/actions/workflows/modernization-contracts.yml).
- **Dependencies:** bundled libraries have their own compatibility patches. Do not bulk-replace vendor trees without a fresh compiler matrix and functional tests. See [dependency audit](doc/DEPENDENCY-AUDIT-2026-10-29.md).
- **Runtime integration:** successful compilation and startup without a database are not proof of gameplay or persistence parity; MariaDB-backed integration coverage is a separate workstream.

### Table of Contents
1. [Prerequisites](#1-prerequisites)
2. [Installation](#2-installation)
3. [Troubleshooting](#3-troubleshooting)
4. [More Documentation](#4-more-documentation)
5. [How to Contribute](#5-how-to-contribute)
6. [License](#6-license)

## 1. Prerequisites
Before installing rAthena there are certain tools and applications you will need which
differs between the varying operating systems available.

### Hardware
Hardware Type | Minimum | Recommended
------|------|------
CPU | 1 Core | 2 Cores
RAM | 1 GB | 2 GB
Disk Space | 300 MB | 500 MB

### Operating System & Preferred Compiler
Operating System | Compiler
------|------
Linux  | GCC 14+ or Clang 18+ / [Make](https://www.gnu.org/software/make/)
Windows | Visual Studio 2022 17.8+ (MSVC 19.38+)

### Required Applications
Application | Name
------|------
Database | [MySQL 5 or newer](https://www.mysql.com/downloads/) / [MariaDB 5 or newer](https://downloads.mariadb.org/)
Database client | [MariaDB Connector/C 3.4.x](https://mariadb.com/downloads/connectors/) (recommended; `libmariadb-dev` on Debian/Ubuntu, `mariadb-connector-c-devel` on Fedora/RHEL, MSI on Windows) or [MySQL Connector/C 8.0+](https://dev.mysql.com/downloads/connector/c/). MariaDB Connector/C >= 3.0.8 is required to log in to MySQL 8.4+/MariaDB 11+ servers
Git | [Windows](https://gitforwindows.org/) / [Linux](https://git-scm.com/download/linux)

### Optional Applications
Application | Name
------|------
Database | [MySQL Workbench 5 or newer](http://www.mysql.com/downloads/workbench/)

## 2. Installation 

### Full Installation Instructions
  * [Windows](https://github.com/rathena/rathena/wiki/Install-on-Windows)
  * [CentOS](https://github.com/rathena/rathena/wiki/Install-on-Centos)
  * [Debian](https://github.com/rathena/rathena/wiki/Install-on-Debian)
  * [FreeBSD](https://github.com/rathena/rathena/wiki/Install-on-FreeBSD)

## 3. Troubleshooting

If you're having problems with starting your server, the first thing you should
do is check what's happening on your consoles. More often that not, all support issues
can be solved simply by looking at the error messages given. Check out the [wiki](https://github.com/rathena/rathena/wiki)
or [forums](https://rathena.org/board) if you need more support on troubleshooting.

## 4. More Documentation
rAthena has a large collection of help files and sample NPC scripts located in the /doc/
directory. These include detailed explanations of NPC script commands, atcommands (@),
group permissions, item bonuses, and packet structures, among many other topics. We
recommend that all users take the time to look over this directory before asking for
assistance elsewhere.

## 5. How to Contribute
Details on how to contribute to rAthena can be found in [CONTRIBUTING.md](https://github.com/rathena/rathena/blob/master/.github/CONTRIBUTING.md)!

## 6. License
Copyright (c) rAthena Development Team - Licensed under [GNU General Public License v3.0](https://github.com/rathena/rathena/blob/master/LICENSE)
