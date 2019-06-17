Contributing to the aktualizr project
=====

We welcome code contributions from the community and are very happy to receive feedback, issue reports and code in the form of pull requests.

Issue Tracker
----

This project uses the Github issue tracker. Please report bugs, issues and feature requests there or email us at otaconnect.support@here.com.

Code quality and style
----

All code should be developed according to the [Google C++ style guide](https://google.github.io/styleguide/cppguide.html). In addition, the code should conform to the following guidelines:

   * Code should be covered by tests
      - Wherever possible, automated testing should be used
      - Tests cases should at least exercise all documented requirements
      - If automated testing is not possible, manual test cases should be described
   * It must be easy for a developer checking out a project to run the test suite based on the information in the [readme](README.adoc)
   * All code must pass all unit tests before a merge request is made
      - Tests that don't pass should be marked pending (with justification) or should be fixed.
   * All code must pass formatting and static linting tests
   * Features should be developed in feature branches
   * Only working code should go into the master branch.
      - master should always be in a deployable state
      - Undeployable code should stay on feature branches
         - Code failing the active unit or integration tests for a project is undeployable
         - Functionally incomplete code is not necessarily undeployable
   * Feature branches should only contain single features
      - Developers should not create large, month-long, multiple-feature branches
      - Developers should try to have their code merged at least once every week
   * All code must be reviewed before it is merged to a master branch.
      - The reviewer can not be the code author
      - All the reviewer's concerns must be resolved to reviewer's satisfaction
      - Reviews should enforce the coding guidelines for the project
   * Bugs reported against code on a master branch should be reproduced with a failing test case before they are resolved
      - Reviewers of code including bug fixes should check that a covering test has been included with the fix
   * New features, bug fixes, and removed functionality should be documented in the [changelog](CHANGELOG.md)

Making a Pull Request
----

When you start developing a feature, please create a feature branch that includes the type of branch, the ID of the issue or ticket if available, and a brief description. For example `feat/9/https-support`, `fix/OTA-123/fix-token-expiry` or `refactor/tidy-up-imports`. Please do not mix feature development, bugfixes and refactoring into the same branch.

When your feature is ready, push the branch and make a pull request. We will review the request and give you feedback. Once the code passes the review it can be merged into master and the branch can be deleted.

Developer Certificate of Origin (DCO)
----

All commits in pull requests must contain a `Signed-off-by:` line to indicate that the developer has agreed to the terms of the [Developer Certificate of Origin](https://developercertificate.org) (see [readme](README.adoc) for more details). A simple way to achieve that is to use the `-s` flag of `git commit`.

New pull requests will automatically be checked by the [probot/dco](https://probot.github.io/apps/dco/).
