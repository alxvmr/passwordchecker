# PasswordChecker
> [!WARNING]
> The app is in development

The application allows you to notify the user at a set time and frequency of password expiry

<p align="center">
    <img src="https://psv4.userapi.com/s/v1/d/aYz4CIC0qqkz7YAzRrO-AKu9-Zc2V-pog8gOlTSpQT8fecQMEjF8DWuQt0CEGIxHdZbogDOoJkCnjaFCrV8pHuQ_mn5LGV5uZTUxxAhWFn19mYYP-VfuoQ/Group_4_1.png">
</p>

The application was developed for [ALT Linux distributions](https://getalt.org/en/)

# About the application
The application includes 2 executable files: `passwordchecker` and `password-checker`

- **`passwordchecker`** - systemd unit.
  Responsible for:
  - sending requests to LDAP
  - time checking
  - starting timers for notifications

- **`password-checker`** - GUI interface for passwordchecker configuration.
  - Sets the settings for LDAP search
  - Sets the time settings for sending notifications (how many alerts to send and at what intervals)

The GUI is provided in 2 variants:
- **`password-checker`** - uses only GTK4 for UI
- **`password-checker-gnome`** - uses Adwaita library for UI

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
In the root directory, create a build folder and run `cmake` and `make`.\
To build the version with **Adwaita**:
```bash
mkdir build
cd build
cmake -DUSE_ADWAITA=ON ..
make
make install
```
To build the version with **only GTK4**:
```bash
mkdir build
cd build
cmake -DUSE_ADWAITA=OFF ..
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
After the RPM package is built, the following packages will be created:
* `password-checker-common-<version>-<release>.<arch>.rpm` - provides common files for password-checker and password-checker-gnome (daemon, desktop file, icons, translations, ...);
* `password-checker-<version>-<release>.<arch>.rpm` - provides an application binary with only GTK4 version;
* `password-checker-gnome-<version>-<release>.<arch>.rpm` - provides an application binary with Adwaita version.

> [!WARNING]
> The `password-checker-common` package is required for the rest of the packages to work

> [!WARNING]
> If `password-checker` and `password-checker-gnome` packages are installed on the system at the same time, the binary from `password-checker-gnome` will be used thanks to the alternatives mechanism