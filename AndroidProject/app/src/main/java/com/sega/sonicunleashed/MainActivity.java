package com.sega.sonicunleashed;

import android.app.Activity;
import android.content.InputDevice;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.Manifest;
import android.view.InputEvent;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.content.res.AssetManager;
import android.util.Log;

/**
 * MainActivity — ponto de entrada do Sonic Unleashed Port para Android.
 *
 * CORREÇÕES aplicadas:
 *  - nativeSetStoragePaths() chamado antes de nativeOnCreate()
 *  - surfaceChanged() passa Surface + dimensões (corrige recriação do swapchain)
 *  - dispatchKeyEvent() repassa eventos de controle físico ao C++
 *  - dispatchGenericMotionEvent() repassa eixos analógicos ao C++
 *  - nativeUpdateVibration() chamado periodicamente
 */
public class MainActivity extends Activity implements SurfaceHolder.Callback {

    private static final String TAG = "SonicPort";
    private static final int    PERM_REQUEST_STORAGE = 1001;

    private SurfaceView mSurfaceView;
    private Vibrator    mVibrator;
    private Surface     mCurrentSurface;

    static {
        System.loadLibrary("sonicengine");
    }

    // -----------------------------------------------------------------------
    // Ciclo de vida
    // -----------------------------------------------------------------------

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        enableImmersiveMode();

        mSurfaceView = new SurfaceView(this);
        mSurfaceView.getHolder().addCallback(this);
        setContentView(mSurfaceView);

        // Vibrador
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager vm = (VibratorManager) getSystemService(VIBRATOR_MANAGER_SERVICE);
            mVibrator = vm != null ? vm.getDefaultVibrator() : null;
        } else {
            mVibrator = (Vibrator) getSystemService(VIBRATOR_SERVICE);
        }

        // CORREÇÃO: passa caminhos de armazenamento ANTES de nativeOnCreate
        nativeSetStoragePaths(
            getFilesDir().getAbsolutePath(),
            getExternalFilesDir(null) != null
                ? getExternalFilesDir(null).getAbsolutePath()
                : getFilesDir().getAbsolutePath()
        );

        nativeSetAssetManager(getAssets());
        nativeOnCreate();

        requestStoragePermission();
        Log.i(TAG, "onCreate concluido");
    }

    @Override
    protected void onResume() {
        super.onResume();
        enableImmersiveMode();
        nativeOnResume();
        Log.d(TAG, "onResume");
    }

    @Override
    protected void onPause() {
        nativeOnPause();
        super.onPause();
        Log.d(TAG, "onPause");
    }

    @Override
    protected void onDestroy() {
        nativeOnDestroy();
        super.onDestroy();
        Log.d(TAG, "onDestroy");
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        Log.w(TAG, "onTrimMemory level=" + level);
        nativeOnTrimMemory(level);
    }

    @Override
    public void onBackPressed() {
        nativeOnBackPressed();
    }

    // -----------------------------------------------------------------------
    // SurfaceHolder.Callback
    // -----------------------------------------------------------------------

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        mCurrentSurface = holder.getSurface();
        Log.i(TAG, "surfaceCreated");
        nativeSurfaceCreated(mCurrentSurface);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "surfaceChanged: " + width + "x" + height);
        mCurrentSurface = holder.getSurface();
        // CORREÇÃO: passa surface + dimensões para que o C++ recrie o swapchain
        nativeSurfaceChanged(mCurrentSurface, width, height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "surfaceDestroyed");
        mCurrentSurface = null;
        nativeSurfaceDestroyed();
    }

    // -----------------------------------------------------------------------
    // Controle físico via KeyEvent (botões)
    // -----------------------------------------------------------------------

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (isGamepadSource(event.getSource())) {
            nativeOnKeyEvent(event.getAction(), event.getKeyCode());
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    // -----------------------------------------------------------------------
    // Controle físico via MotionEvent (eixos analógicos)
    // -----------------------------------------------------------------------

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (isGamepadSource(event.getSource())) {
            float lx = getAxisValue(event, MotionEvent.AXIS_X);
            float ly = getAxisValue(event, MotionEvent.AXIS_Y);
            float rx = getAxisValue(event, MotionEvent.AXIS_Z);
            float ry = getAxisValue(event, MotionEvent.AXIS_RZ);
            float lt = getAxisValue(event, MotionEvent.AXIS_LTRIGGER);
            float rt = getAxisValue(event, MotionEvent.AXIS_RTRIGGER);
            nativeOnMotionEvent(lx, ly, rx, ry, lt, rt);
            return true;
        }
        return super.dispatchGenericMotionEvent(event);
    }

    private static boolean isGamepadSource(int source) {
        return (source & InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD
            || (source & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK;
    }

    private static float getAxisValue(MotionEvent event, int axis) {
        return event.getAxisValue(axis, 0);
    }

    // -----------------------------------------------------------------------
    // Toque
    // -----------------------------------------------------------------------

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        int action    = event.getActionMasked();
        int ptrIndex  = event.getActionIndex();
        int pointerId = event.getPointerId(ptrIndex);
        float x       = event.getX(ptrIndex);
        float y       = event.getY(ptrIndex);
        nativeOnTouchEvent(action, pointerId, x, y);
        return true;
    }

    // -----------------------------------------------------------------------
    // Modo imersivo
    // -----------------------------------------------------------------------

    private void enableImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            WindowInsetsController ctrl = getWindow().getInsetsController();
            if (ctrl != null) {
                ctrl.hide(WindowInsets.Type.statusBars() | WindowInsets.Type.navigationBars());
                ctrl.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
            );
        }
    }

    // -----------------------------------------------------------------------
    // Permissões
    // -----------------------------------------------------------------------

    private void requestStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M &&
            Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
                    != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(
                    new String[]{ Manifest.permission.READ_EXTERNAL_STORAGE,
                                  Manifest.permission.WRITE_EXTERNAL_STORAGE },
                    PERM_REQUEST_STORAGE);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode,
                                           String[] permissions,
                                           int[] grantResults) {
        if (requestCode == PERM_REQUEST_STORAGE) {
            if (grantResults.length > 0 &&
                grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                Log.i(TAG, "Permissao de armazenamento concedida");
            } else {
                Log.w(TAG, "Permissao de armazenamento negada — saves podem falhar");
            }
        }
    }

    /** Chamado por C++ via nativeCheckStoragePermission() */
    public boolean hasStoragePermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) return true;
        return checkSelfPermission(Manifest.permission.READ_EXTERNAL_STORAGE)
               == PackageManager.PERMISSION_GRANTED;
    }

    /** Chamado por C++ via nativeGetSavePath() */
    public String getExternalStoragePath() {
        if (getExternalFilesDir(null) != null)
            return getExternalFilesDir(null).getAbsolutePath();
        return getFilesDir().getAbsolutePath();
    }

    // -----------------------------------------------------------------------
    // Vibração
    // -----------------------------------------------------------------------

    public void performVibrate(long durationMs) {
        if (mVibrator == null || !mVibrator.hasVibrator()) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mVibrator.vibrate(VibrationEffect.createOneShot(
                durationMs, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            mVibrator.vibrate(durationMs);
        }
    }

    // -----------------------------------------------------------------------
    // Declarações JNI (implementadas em jni_bridge.cpp)
    // -----------------------------------------------------------------------

    private native void    nativeOnCreate();
    private native void    nativeOnDestroy();
    private native void    nativeOnPause();
    private native void    nativeOnResume();
    private native void    nativeOnTrimMemory(int level);
    private native void    nativeOnBackPressed();
    private native void    nativeSurfaceCreated(Surface surface);
    private native void    nativeSurfaceChanged(Surface surface, int width, int height);
    private native void    nativeSurfaceDestroyed();
    private native void    nativeSetAssetManager(AssetManager am);
    private native void    nativeSetStoragePaths(String internalPath, String externalPath);
    private native void    nativeOnTouchEvent(int action, int pointerId, float x, float y);
    private native void    nativeOnKeyEvent(int action, int keyCode);
    private native void    nativeOnMotionEvent(float lx, float ly, float rx, float ry,
                                               float lt, float rt);
    private native void    nativeOnGamepadConnected(boolean connected);
    private native void    nativeVibrate(long durationMs);
    private native void    nativeUpdateVibration();
    private native boolean nativeCheckStoragePermission();
    private native String  nativeGetSavePath();
}
