package com.openbingo.app

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.ActivityInfo
import android.os.Bundle
import android.print.PrintAttributes
import android.print.PrintManager
import android.webkit.JavascriptInterface
import android.webkit.WebView
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.ActivityResultLauncher
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

private class PrintBridge(private val activity: MainActivity, private val webView: WebView) {
  @JavascriptInterface
  fun print() {
    activity.runOnUiThread {
      val printManager = activity.getSystemService(Context.PRINT_SERVICE) as PrintManager
      val adapter = webView.createPrintDocumentAdapter("Open Bingo")
      printManager.print("Open Bingo", adapter, PrintAttributes.Builder().build())
    }
  }
}

private class OrientationBridge(private val activity: MainActivity) {
  @JavascriptInterface
  fun lockLandscape() {
    activity.runOnUiThread {
      activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
    }
  }
  @JavascriptInterface
  fun unlock() {
    activity.runOnUiThread {
      activity.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED
    }
  }
}

private class SaveBridge(private val activity: MainActivity) {
  @JavascriptInterface
  fun saveFile(content: String, filename: String) {
    activity.runOnUiThread {
      activity.pendingSaveContent = content
      val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
        addCategory(Intent.CATEGORY_OPENABLE)
        type = "application/json"
        putExtra(Intent.EXTRA_TITLE, filename)
      }
      activity.saveFileLauncher.launch(intent)
    }
  }
}

class MainActivity : TauriActivity() {
  var pendingSaveContent: String? = null
  lateinit var saveFileLauncher: ActivityResultLauncher<Intent>

  override fun onCreate(savedInstanceState: Bundle?) {
    enableEdgeToEdge()
    WindowCompat.setDecorFitsSystemWindows(window, false)
    saveFileLauncher = registerForActivityResult(
      ActivityResultContracts.StartActivityForResult()
    ) { result ->
      if (result.resultCode == Activity.RESULT_OK) {
        result.data?.data?.let { uri ->
          pendingSaveContent?.let { content ->
            contentResolver.openOutputStream(uri)?.use { stream ->
              stream.write(content.toByteArray(Charsets.UTF_8))
            }
            pendingSaveContent = null
          }
        }
      }
    }
    super.onCreate(savedInstanceState)
  }

  override fun onWindowFocusChanged(hasFocus: Boolean) {
    super.onWindowFocusChanged(hasFocus)
    if (hasFocus) hideSystemUI()
  }

  private fun hideSystemUI() {
    WindowInsetsControllerCompat(window, window.decorView).let { ctrl ->
      ctrl.hide(WindowInsetsCompat.Type.systemBars())
      ctrl.systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
  }

  override fun onWebViewCreate(webView: WebView) {
    webView.overScrollMode = WebView.OVER_SCROLL_NEVER
    webView.addJavascriptInterface(PrintBridge(this, webView), "AndroidPrint")
    webView.addJavascriptInterface(SaveBridge(this), "AndroidSave")
    webView.addJavascriptInterface(OrientationBridge(this), "AndroidOrientation")
  }
}
