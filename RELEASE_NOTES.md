# v1.4.2

- Successful first-run file setup now records a completion token in the plugin TOML and is skipped on later launches.
- Existing installations and user settings are preserved; unsuccessful setup can be retried.
- Older Melee Splash installation tokens are upgraded after validating the existing data.
- Removed startup settings and stat-ID dumps from the log.
- Installation now handles reordered data columns by name and reports missing populated columns before writing incomplete rows.
