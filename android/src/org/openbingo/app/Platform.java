package org.openbingo.app;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInstaller;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.content.pm.ActivityInfo;
import android.print.PageRange;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintDocumentInfo;
import android.print.PrintManager;
import android.provider.MediaStore;
import android.view.View;
import android.view.WindowManager;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;

// Services natifs appelés depuis C++ via JNI (src/app/platform.cpp) : notification
// locale après un merge distant (SPEC §8) et feuille de partage du lien d'appairage.
// Aucune référence à la classe R générée : les ressources sont résolues par nom,
// pour ne dépendre d'aucun namespace Gradle.
public class Platform {

    public static final String CHANNEL_ID = "openbingo.sync";
    public static final String CHANNEL_VEILLE_ID = "openbingo.veille";
    private static final int    NOTIFICATION_ID = 4545;
    private static final int    PERMISSION_REQUEST = 4545;

    public static void createChannel(Context ctx) {
        if (ctx == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.O)
            return;
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        if (nm == null)
            return;

        if (nm.getNotificationChannel(CHANNEL_ID) == null) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID, "Synchronisation", NotificationManager.IMPORTANCE_DEFAULT);
            channel.setDescription("Changements reçus sur vos projets");
            nm.createNotificationChannel(channel);
        }

        if (nm.getNotificationChannel(CHANNEL_VEILLE_ID) == null) {
            NotificationChannel veille = new NotificationChannel(
                    CHANNEL_VEILLE_ID, "Veille en arrière-plan",
                    NotificationManager.IMPORTANCE_MIN);
            veille.setDescription("Service discret quand l'app est en arrière-plan");
            veille.setShowBadge(false);
            nm.createNotificationChannel(veille);
        }
    }

    // Android 13+ : POST_NOTIFICATIONS est une permission runtime. Sans activité
    // (cas d'un service), on ne peut pas la demander : on sort silencieusement.
    public static void requestPermission(Context ctx) {
        if (!(ctx instanceof Activity) || Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
            return;
        Activity activity = (Activity) ctx;
        if (activity.checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS)
                != PackageManager.PERMISSION_GRANTED) {
            activity.requestPermissions(
                    new String[]{ android.Manifest.permission.POST_NOTIFICATIONS },
                    PERMISSION_REQUEST);
        }
    }

    public static void showNotification(Context ctx, String title, String body) {
        showNotification(ctx, title, body, 0L);
    }

    // ntfy : démarre ou arrête la veille push (topics = bingo-{channelTag}).
    public static void configurePush(Context ctx, String baseUrl, String[] topics,
                                     String deviceId) {
        PushService.configure(ctx, baseUrl, topics, deviceId);
    }

    // whenMs > 0 : horodatage de la notification = heure de la modification
    // (pas l'heure de réception). setShowWhen(true) pour l'afficher dans le tiroir.
    public static void showNotification(Context ctx, String title, String body,
                                        long whenMs) {
        if (ctx == null)
            return;
        createChannel(ctx);
        NotificationManager nm = ctx.getSystemService(NotificationManager.class);
        if (nm == null)
            return;

        Notification.Builder builder = new Notification.Builder(ctx, CHANNEL_ID)
                .setSmallIcon(smallIcon(ctx))
                .setContentTitle(title)
                .setContentText(body)
                .setStyle(new Notification.BigTextStyle().bigText(body))
                .setAutoCancel(true);

        if (whenMs > 0) {
            builder.setWhen(whenMs);
            builder.setShowWhen(true);
        }

        // Tap sur la notif → ouvre l'app.
        Intent open = ctx.getPackageManager().getLaunchIntentForPackage(ctx.getPackageName());
        if (open != null) {
            open.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_CLEAR_TOP);
            builder.setContentIntent(PendingIntent.getActivity(
                    ctx, 0, open,
                    PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE));
        }

        nm.notify(NOTIFICATION_ID, builder.build());
    }

    // Feuille de partage native : le lien d'appairage part dans WhatsApp, SMS, mail…
    public static boolean shareText(Context ctx, String text) {
        if (ctx == null)
            return false;
        try {
            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("text/plain");
            send.putExtra(Intent.EXTRA_TEXT, text);
            Intent chooser = Intent.createChooser(send, "Partager la liste");
            // Hors d'une Activity, le chooser exige sa propre tâche.
            chooser.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(chooser);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    // Classement PNG : insert MediaStore (pas de FileProvider) puis feuille de partage.
    public static boolean shareImage(Context ctx, String path) {
        if (ctx == null || path == null)
            return false;
        File file = new File(path);
        if (!file.isFile() || file.length() == 0)
            return false;
        try {
            ContentValues values = new ContentValues();
            values.put(MediaStore.Images.Media.DISPLAY_NAME, file.getName());
            values.put(MediaStore.Images.Media.MIME_TYPE, "image/png");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                values.put(MediaStore.Images.Media.IS_PENDING, 1);

            Uri collection = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q)
                collection = MediaStore.Images.Media.getContentUri(
                        MediaStore.VOLUME_EXTERNAL_PRIMARY);

            Uri uri = ctx.getContentResolver().insert(collection, values);
            if (uri == null)
                return false;

            try (InputStream in = new FileInputStream(file);
                 OutputStream out = ctx.getContentResolver().openOutputStream(uri)) {
                if (out == null)
                    return false;
                byte[] buffer = new byte[65536];
                int read;
                while ((read = in.read(buffer)) > 0)
                    out.write(buffer, 0, read);
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                values.clear();
                values.put(MediaStore.Images.Media.IS_PENDING, 0);
                ctx.getContentResolver().update(uri, values, null, null);
            }

            Intent send = new Intent(Intent.ACTION_SEND);
            send.setType("image/png");
            send.putExtra(Intent.EXTRA_STREAM, uri);
            send.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            Intent chooser = Intent.createChooser(send, "Partager le classement");
            chooser.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(chooser);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    // --- Agenda ---
    // ACTION_INSERT sur le calendrier : l'app d'agenda s'ouvre pré-remplie (titre,
    // description, journée entière) et c'est l'utilisateur qui confirme. Aucune
    // permission calendrier requise — on n'écrit rien nous-mêmes.
    public static boolean addCalendarEvent(Context ctx, String title,
                                           String description, long startMs) {
        if (ctx == null)
            return false;
        try {
            Intent intent = new Intent(Intent.ACTION_INSERT)
                .setData(android.provider.CalendarContract.Events.CONTENT_URI)
                .putExtra(android.provider.CalendarContract.Events.TITLE, title)
                .putExtra(android.provider.CalendarContract.Events.DESCRIPTION, description)
                .putExtra(android.provider.CalendarContract.EXTRA_EVENT_BEGIN_TIME, startMs)
                .putExtra(android.provider.CalendarContract.EXTRA_EVENT_END_TIME,
                          startMs + 24L * 3600 * 1000)
                .putExtra(android.provider.CalendarContract.EXTRA_EVENT_ALL_DAY, true);
            intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(intent);
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    // Impression système (PrintManager) à partir d'un PDF déjà généré — sans
    // FileProvider : on streame le fichier dans le job d'impression.
    public static boolean printPdf(Context ctx, final String pdfPath) {
        if (!(ctx instanceof Activity) || pdfPath == null)
            return false;
        final File pdf = new File(pdfPath);
        if (!pdf.isFile() || pdf.length() == 0)
            return false;

        final Activity activity = (Activity) ctx;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                PrintManager pm = (PrintManager) activity.getSystemService(Context.PRINT_SERVICE);
                if (pm == null)
                    return;
                PrintDocumentAdapter adapter = new PrintDocumentAdapter() {
                    @Override
                    public void onLayout(PrintAttributes oldAttributes,
                                         PrintAttributes newAttributes,
                                         CancellationSignal cancellationSignal,
                                         LayoutResultCallback callback,
                                         Bundle extras) {
                        if (cancellationSignal.isCanceled()) {
                            callback.onLayoutCancelled();
                            return;
                        }
                        PrintDocumentInfo info = new PrintDocumentInfo.Builder("openbingo.pdf")
                                .setContentType(PrintDocumentInfo.CONTENT_TYPE_DOCUMENT)
                                .setPageCount(PrintDocumentInfo.PAGE_COUNT_UNKNOWN)
                                .build();
                        callback.onLayoutFinished(info, true);
                    }

                    @Override
                    public void onWrite(PageRange[] pages,
                                        ParcelFileDescriptor destination,
                                        CancellationSignal cancellationSignal,
                                        WriteResultCallback callback) {
                        try (InputStream in = new FileInputStream(pdf);
                             OutputStream out = new FileOutputStream(destination.getFileDescriptor())) {
                            byte[] buffer = new byte[65536];
                            int read;
                            while ((read = in.read(buffer)) > 0) {
                                if (cancellationSignal.isCanceled()) {
                                    callback.onWriteCancelled();
                                    return;
                                }
                                out.write(buffer, 0, read);
                            }
                            callback.onWriteFinished(new PageRange[]{ PageRange.ALL_PAGES });
                        } catch (Exception e) {
                            callback.onWriteFailed(e.getMessage());
                        }
                    }
                };
                pm.print("Open Bingo", adapter, null);
            }
        });
        return true;
    }

    // --- Mise à jour depuis l'app ---
    //
    // PackageInstaller plutôt qu'un Intent ACTION_VIEW sur un content:// : celui-ci
    // imposerait un FileProvider (donc une dépendance androidx et une autorité
    // déclarée). Ici on écrit l'APK dans une session d'installation, et Android
    // affiche lui-même sa demande de confirmation — rien ne s'installe en douce.
    public static boolean installApk(Context ctx, String apkPath) {
        if (ctx == null || apkPath == null)
            return false;

        File apk = new File(apkPath);
        if (!apk.isFile() || apk.length() == 0)
            return false;

        PackageInstaller.Session session = null;
        try {
            PackageInstaller installer = ctx.getPackageManager().getPackageInstaller();
            PackageInstaller.SessionParams params = new PackageInstaller.SessionParams(
                    PackageInstaller.SessionParams.MODE_FULL_INSTALL);

            int sessionId = installer.createSession(params);
            session = installer.openSession(sessionId);

            try (InputStream in = new FileInputStream(apk);
                 OutputStream out = session.openWrite("openbingo", 0, apk.length())) {
                byte[] buffer = new byte[65536];
                int read;
                while ((read = in.read(buffer)) > 0)
                    out.write(buffer, 0, read);
                session.fsync(out);
            }

            // Android répond de façon asynchrone : pour une app hors Play Store, la
            // première réponse est STATUS_PENDING_USER_ACTION, qui porte l'écran de
            // confirmation à afficher. Sans ce receveur, l'installation resterait
            // silencieusement en attente.
            Intent status = new Intent(ACTION_INSTALL_STATUS).setPackage(ctx.getPackageName());
            int flags = PendingIntent.FLAG_UPDATE_CURRENT;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S)
                flags |= PendingIntent.FLAG_MUTABLE;   // Android remplit l'Intent de réponse

            PendingIntent pending = PendingIntent.getBroadcast(ctx, sessionId, status, flags);
            session.commit(pending.getIntentSender());
            return true;

        } catch (Exception e) {
            if (session != null)
                session.abandon();
            return false;
        } finally {
            if (session != null)
                session.close();
        }
    }

    public static final String ACTION_INSTALL_STATUS = "org.openbingo.app.INSTALL_STATUS";

    // Déclaré dans AndroidManifest.xml. Reçoit l'avancement de la session et ouvre
    // l'écran de confirmation système quand Android le demande.
    public static class InstallReceiver extends BroadcastReceiver {
        @Override
        public void onReceive(Context ctx, Intent intent) {
            int status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS,
                                            PackageInstaller.STATUS_FAILURE);
            if (status != PackageInstaller.STATUS_PENDING_USER_ACTION)
                return;   // succès, échec ou annulation : Android a déjà informé l'utilisateur

            Intent confirm = intent.getParcelableExtra(Intent.EXTRA_INTENT);
            if (confirm == null)
                return;
            // Le receveur n'est pas une Activity : l'écran de confirmation a besoin
            // de sa propre tâche.
            confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            ctx.startActivity(confirm);
        }
    }

    // Vibration courte : en mode Courses, on coche sans quitter le rayon des yeux.
    public static void vibrate(Context ctx, int ms) {
        if (ctx == null)
            return;
        try {
            Vibrator vibrator;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                VibratorManager manager = ctx.getSystemService(VibratorManager.class);
                vibrator = (manager != null) ? manager.getDefaultVibrator() : null;
            } else {
                vibrator = (Vibrator) ctx.getSystemService(Context.VIBRATOR_SERVICE);
            }
            if (vibrator == null || !vibrator.hasVibrator())
                return;
            vibrator.vibrate(VibrationEffect.createOneShot(
                    ms, VibrationEffect.DEFAULT_AMPLITUDE));
        } catch (Exception e) {
            // Pas de vibreur, permission refusée : ce n'est qu'un confort.
        }
    }

    // Mode Courses : l'écran doit rester allumé, on tient le téléphone sans le toucher
    // pendant des minutes. Les drapeaux de fenêtre ne se posent que sur le thread UI —
    // les toucher depuis le thread Qt lève une exception.
    public static void keepScreenOn(Context ctx, final boolean on) {
        if (!(ctx instanceof Activity))
            return;
        final Activity activity = (Activity) ctx;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (on)
                    activity.getWindow().addFlags(
                            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                else
                    activity.getWindow().clearFlags(
                            WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
            }
        });
    }

    public static void lockLandscape(Context ctx) {
        if (!(ctx instanceof Activity))
            return;
        final Activity activity = (Activity) ctx;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                activity.setRequestedOrientation(
                        ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
            }
        });
    }

    public static void unlockOrientation(Context ctx) {
        if (!(ctx instanceof Activity))
            return;
        final Activity activity = (Activity) ctx;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                activity.setRequestedOrientation(
                        ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED);
            }
        });
    }

    // Mode play plein écran : masquer barres système (comme l'ancienne app Tauri).
    public static void setImmersive(Context ctx, final boolean on) {
        if (!(ctx instanceof Activity))
            return;
        final Activity activity = (Activity) ctx;
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                View decor = activity.getWindow().getDecorView();
                if (on) {
                    decor.setSystemUiVisibility(
                            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
                } else {
                    decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
                }
            }
        });
    }

    private static int smallIcon(Context ctx) {
        int id = ctx.getResources().getIdentifier(
                "ic_stat_notify", "drawable", ctx.getPackageName());
        return id != 0 ? id : android.R.drawable.stat_notify_sync;
    }
}
