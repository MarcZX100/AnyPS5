### What
<!-- Describe the change and the project need. For a runtime/export change, include the affected title and ID, current-main evidence (exact error or log line), and the first missing import or failing behaviour. For other changes, give a small reproduction or explain the concrete need. -->

### Tested
<!-- OS, title or homebrew, hardware if relevant, and what was checked. Include expected and actual results for a reproduction. "Not tested" is a valid answer; explain why when the change affects runtime behaviour. -->

### Checklist
<!-- Rules behind each item: https://github.com/boykopovar/AnyPS5/blob/main/CONTRIBUTING.md -->
- [ ] Based on current `main`; no other open PR implements the same functions
- [ ] One topic per PR; follow-ups go in a new PR
- [ ] Code comments follow [the comment policy](https://github.com/boykopovar/AnyPS5/blob/main/docs/dev/CONVENTIONS.md): technical-debt comments update TechnicalDebt; closing `#endif` and namespace comments are allowed
- [ ] Unimplemented paths throw (`NotImplemented_nid_no_patch`); silent stubs are listed in [TechnicalDebt](https://github.com/boykopovar/AnyPS5/blob/main/docs/dev/TechnicalDebt.md#silent-stubs)
- [ ] No notes, investigation `.md` files or images added to the repository (attach them to this PR)
- [ ] New third-party code is a submodule built from source
- [ ] Implementation provides general behaviour; title evidence identifies the need and does not cause a title-specific special case
- [ ] Depends on: <!-- #PR, or none -->
- [ ] AI-assisted: <!-- yes / no -->
