# Releasing

1. Set `<revision>` in `pom.xml` to the release version, and commit.
2. Push a tag `v<version>`. GitHub Actions builds, tests against JDK 11, 21 and 25, checks that the tag matches `<revision>`, and creates a GitHub Release with the binaries and `SHA256SUMS.txt`.
3. On the machine with the GPG key and the `central` entry in `~/.m2/settings.xml`, check out the tag and run this in PowerShell:
   ```
   scripts/publish-central.ps1 <version>
   ```
   It downloads the release's binaries, verifies their checksums, and signs and uploads those files. Add `-DryRun` to try it with no signing, deploying to a temporary folder.
4. Approve the deployment at https://central.sonatype.com/publishing/deployments. **Published versions can't be changed or deleted.**
5. Update the version in `docs/packaging.md` and set `<revision>` to the next version.
