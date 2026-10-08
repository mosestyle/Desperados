package com.mosestyle.desperados.original;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.Gravity;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * The app's entry point. The game can only start once the app may read the Desperados folder,
 * so this screen asks for "all files access" first and opens the game as soon as it's allowed
 * (also when the user comes back from the settings screen).
 */
public class StartActivity extends Activity {

    private boolean askedOnce;

    private boolean hasAccess() {
        if (Build.VERSION.SDK_INT >= 30) return Environment.isExternalStorageManager();
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE) == PackageManager.PERMISSION_GRANTED;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        LinearLayout box = new LinearLayout(this);
        box.setOrientation(LinearLayout.VERTICAL);
        box.setGravity(Gravity.CENTER);
        box.setBackgroundColor(Color.BLACK);
        box.setPadding(64, 64, 64, 64);
        TextView text = new TextView(this);
        text.setTextColor(Color.WHITE);
        text.setTextSize(18);
        text.setGravity(Gravity.CENTER);
        text.setText("Desperados needs access to the 'Desperados' folder on your phone's storage.\n\n"
                + "Tap the button, switch on 'Allow access to manage all files' for Desperados Original, "
                + "then come back. The game starts by itself.");
        Button button = new Button(this);
        button.setText("Allow file access");
        button.setOnClickListener(v -> askForAccess());
        box.addView(text);
        box.addView(button);
        setContentView(box);
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (hasAccess()) {
            startGame();
        } else if (!askedOnce) {
            askedOnce = true;
            askForAccess();
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] results) {
        super.onRequestPermissionsResult(requestCode, permissions, results);
        if (hasAccess()) startGame();
    }

    private void askForAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            try {
                startActivity(new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                        Uri.parse("package:" + getPackageName())));
            } catch (Exception e) {
                try {
                    startActivity(new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION));
                } catch (Exception ignored) {
                }
            }
        } else {
            requestPermissions(new String[] { Manifest.permission.READ_EXTERNAL_STORAGE }, 1);
        }
    }

    private void startGame() {
        Intent intent = new Intent(this, OriginalActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
        startActivity(intent);
        finish();
    }
}
