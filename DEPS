vars = {
  "dart_branch": "/branches/bleeding_edge",
  "dart_rev": "",
  
  "chromium_url": "http://src.chromium.org/svn",
  "chromium_git": "http://git.chromium.org/git",

  "gyp_rev": "@1752",
  "nss_rev": "@209026",

  "co19_rev": "@623",
  "co19_repo": "http://co19.googlecode.com/svn/trunk/co19/",

  "skia_branch": "/trunk",
  "skia_rev": "@11999",
}

deps = {
  # dart standalone (https://dart.googlecode.com/svn/trunk/deps/standalone.deps/DEPS)
  "openglui/third_party/dart":
    "http://dart.googlecode.com/svn" + Var("dart_branch") + "/dart" + Var("dart_rev"),

  # Stuff needed for GYP to run.
  "openglui/third_party/dart/third_party/gyp":
    "http://gyp.googlecode.com/svn/trunk" + Var("gyp_rev"),

  # Stuff needed for secure sockets.
  "openglui/third_party/dart/third_party/nss":
      Var("chromium_url") + "/trunk/deps/third_party/nss" + Var("nss_rev"),

  "openglui/third_party/dart/third_party/sqlite":
      Var("chromium_url") + "/trunk/src/third_party/sqlite" + Var("nss_rev"),

  "openglui/third_party/dart/third_party/zlib":
      Var("chromium_url") + "/trunk/src/third_party/zlib" + Var("nss_rev"),

  "openglui/third_party/dart/third_party/net_nss":
      Var("chromium_url") + "/trunk/src/net/third_party/nss" + Var("nss_rev"),

  # skia standalone (http://skia.googlecode.com/svn/trunk/DEPS)
  "openglui/third_party/skia" : "http://skia.googlecode.com/svn" + Var("skia_branch") + Var("skia_rev"),
  "openglui/third_party/skia/third_party/externals/angle" : "https://chromium.googlesource.com/external/angleproject.git@e574e26f48223a6718feab841b4a7720785b497a",
  "openglui/third_party/skia/third_party/externals/fontconfig" : "https://skia.googlesource.com/third_party/fontconfig.git@2.10.93",
  "openglui/third_party/skia/third_party/externals/freetype" : "https://skia.googlesource.com/third_party/freetype2.git@VER-2-5-0-1",
  "openglui/third_party/skia/third_party/externals/gyp" : "https://chromium.googlesource.com/external/gyp.git@0635a6e266eb4cdd01e942331997f76b3737be79",
  "openglui/third_party/skia/third_party/externals/iconv" : "https://skia.googlesource.com/third_party/libiconv.git@v1.14",
  "openglui/third_party/skia/third_party/externals/libjpeg" : "https://chromium.googlesource.com/chromium/deps/libjpeg_turbo.git@82ce8a6d4ebe12a177c0c3597192f2b4f09e81c3",
  "openglui/third_party/skia/third_party/externals/jsoncpp" : "https://chromium.googlesource.com/external/jsoncpp/jsoncpp.git@ab1e40f3bce061ea6f9bdc60351d6cde2a4f872b",
  # Move to a webp version newer than 0.3.0 to pickup NEON fix for iOS
  "openglui/third_party/skia/third_party/externals/jsoncpp-chromium" : "https://chromium.googlesource.com/chromium/src/third_party/jsoncpp.git@41239939c0c60481f34887d52c038facf05f5533",
  "openglui/third_party/skia/third_party/externals/libwebp" : "https://chromium.googlesource.com/webm/libwebp.git@3fe91635df8734b23f3c1b9d1f0c4fa8cfaf4e39",
  "openglui/third_party/skia/third_party/externals/poppler" : "https://skia.googlesource.com/third_party/poppler.git@poppler-0.22.5",

}

deps_os = {
  "win": {
    "openglui/third_party/cygwin":
      Var("chromium_url") + "/trunk/deps/third_party/cygwin@66844",
  },

  "android": {

    #skia
    "openglui/third_party/skia/platform_tools/android/third_party/externals/expat" : "https://android.googlesource.com/platform/external/expat.git@android-4.2.2_r1.2",
    "openglui/third_party/skia/platform_tools/android/third_party/externals/gif" : "https://android.googlesource.com/platform/external/giflib.git@android-4.2.2_r1.2",
    "openglui/third_party/skia/platform_tools/android/third_party/externals/png" : "https://android.googlesource.com/platform/external/libpng.git@android-4.2.2_r1.2",
    "openglui/third_party/skia/platform_tools/android/third_party/externals/jpeg" : "https://android.googlesource.com/platform/external/jpeg.git@android-4.2.2_r1.2",

    # dart
    "openglui/third_party/android_tools":
      Var("chromium_git") + "/android_tools.git" +
      "@ebd740fc3d55dda34d9322c8c5f7749302734325",
  },
}

hooks = [
  {
    "pattern": ".",
    "action": ["python", "openglui/tools/generate_projects.py", "samples"],
  },
]
