# PasswordChecker
> [!WARNING]
> The app is in development

The application allows you to notify the user at a set time and frequency of password expiry

<p align="center">
    <img src="https://psv4.userapi.com/s/v1/d/2XM55r9gDpWa4C2NtLrKB6R7kAXSqRKMYkTwxvr3P_M_bAa1wCkgwcEtzHH1C6mVPPfvev5btwmhoaa_2L8lqwDsKzMjEOsV5IcfzieX_-ISh3haKUXBrQ/Pas.png">
</p>

The application was developed for [ALT Linux distributions](https://getalt.org/en/)

---
# About the application
The application includes 2 executable files: `passwordchecker` and `PasswordCheckerSettings`

- **`passwordchecker`** - systemd unit.
  Responsible for:
  - sending requests to LDAP
  - time checking
  - starting timers for notifications

- **`PasswordCheckerSettings`** - GUI interface for passwordchecker configuration.
  - Sets the settings for LDAP search
  - Sets the time settings for sending notifications (how many alerts to send and at what intervals)

These components are linked through GSettings

<p align="center">
    <img src="https://psv4.userapi.com/s/v1/d/BcQhjGEcvjMsDBPM4D8YHqAakWFMYmiGvdwnFmQBzSgIgGyBUdqMpy_fNK0ExaoTp0L7y07OCSjBSVzjYtwAzVtbKvWwOKFdRIzDavbYcjkljYlQHPq6UQ/Graph.png">
</p>

# Dependencies
```
gcc-c++
gio-2.0
gtk4
libldap-devel
libsasl2-devel
```

# Build
## Local build
In the root directory, create a build folder and run `cmake` and `make`:
```bash
mkdir build
cd build
cmake ..
make
make install
```
After installation, the passwordchecker-user.service must be activated:
```
systemctl --user daemon-reload
systemctl --user start passwordchecker-user.service
```

## RPM package
The `.spec` file for the project is in the repository:
```
.gear/passwordchecker.spec
```