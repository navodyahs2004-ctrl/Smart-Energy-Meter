# flutter_smart_energy_meter

A new Flutter project.

You can Download .apk File from Here(Click the link and install the app to your Smart Phone) :- https://drive.google.com/drive/folders/1dszowQJG5Wjq4MNsExZaOYl9nQFblskc?usp=sharing

## Getting Started

This project is a starting point for a Flutter application.

A few resources to get you started if this is your first Flutter project:

- [Learn Flutter](https://docs.flutter.dev/get-started/learn-flutter)
- [Write your first Flutter app](https://docs.flutter.dev/get-started/codelab)
- [Flutter learning resources](https://docs.flutter.dev/reference/learning-resources)

For help getting started with Flutter development, view the
[online documentation](https://docs.flutter.dev/), which offers tutorials,
samples, guidance on mobile development, and a full API reference.


# Smart Energy Meter Mobile App

This mobile app was developed using **Flutter** and **Dart**.
It is used to monitor and control the ESP32 Smart Energy Meter / Smart Plug through Firebase Realtime Database.

---

## 1. Software Needed

To open, edit, and build this app, install:

* Flutter SDK
* Android Studio
* VS Code or Android Studio editor
* Git
* Android phone or Android emulator

---

## 2. App Technology

| Item                | Details                                    |
| ------------------- | ------------------------------------------ |
| Language            | Dart                                       |
| Framework           | Flutter                                    |
| Database            | Firebase Realtime Database                 |
| Main file           | `lib/main.dart`                            |
| Firebase config     | `lib/firebase_options.dart`                |
| Android permissions | `android/app/src/main/AndroidManifest.xml` |

---

## 3. How to Get the App Code

Open CMD or terminal and run:

```cmd
git clone https://github.com/YOUR_USERNAME/Smart-Energy-Meter.git
```

Then go to the Flutter app folder:

```cmd
cd Smart-Energy-Meter/Flutter_App
```

Install Flutter packages:

```cmd
flutter pub get
```

---

## 4. How to Open and Edit the App

Open the `Flutter_App` folder using **VS Code** or **Android Studio**.

Main app code is here:

```text
lib/main.dart
```

To change app UI, buttons, pages, colors, Firebase paths, or logic, edit:

```text
lib/main.dart
```

If new packages are added, edit:

```text
pubspec.yaml
```

Then run:

```cmd
flutter pub get
```

---

## 5. How to Run the App on a Phone

1. Connect Android phone using USB.
2. Enable Developer Options.
3. Enable USB Debugging.
4. Open CMD inside `Flutter_App` folder.
5. Run:

```cmd
flutter run
```

The app will install and open on the phone.

---

## 6. How to Create APK File

To create an APK file for sharing or installing manually:

```cmd
flutter clean
flutter pub get
flutter build apk --release
```

APK file will be created here:

```text
build/app/outputs/flutter-apk/app-release.apk
```

You can rename it:

```text
SmartEnergyMeter.apk
```

Then share it using Google Drive, WhatsApp, Telegram, or USB.

---

## 7. How to Create Play Store File

For Google Play Store, create an App Bundle:

```cmd
flutter clean
flutter pub get
flutter build appbundle --release
```

The app bundle file will be here:

```text
build/app/outputs/bundle/release/app-release.aab
```

Upload this `.aab` file to Google Play Console.

---

## 8. How Another Developer Can Create a New Version

1. Clone or download the project.
2. Open the `Flutter_App` folder.
3. Run:

```cmd
flutter pub get
```

4. Edit app code in:

```text
lib/main.dart
```

5. Test the app:

```cmd
flutter run
```

6. Build APK:

```cmd
flutter build apk --release
```

7. Share or publish the new APK.

---

## 9. Important Notes

Before making the project public, check and remove private data such as:

* Firebase private configuration
* API keys
* Database URLs if private
* Personal project IDs

For public GitHub projects, use example values or explain that developers must add their own Firebase configuration.
