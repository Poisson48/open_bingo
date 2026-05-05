package com.openbingo.app

import android.content.Context
import android.os.Bundle
import android.print.PrintAttributes
import android.print.PrintManager
import android.webkit.JavascriptInterface
import android.webkit.WebView
import androidx.activity.enableEdgeToEdge

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

class MainActivity : TauriActivity() {
  override fun onCreate(savedInstanceState: Bundle?) {
    enableEdgeToEdge()
    super.onCreate(savedInstanceState)
  }

  override fun onWebViewCreate(webView: WebView) {
    webView.addJavascriptInterface(PrintBridge(this, webView), "AndroidPrint")
  }
}
