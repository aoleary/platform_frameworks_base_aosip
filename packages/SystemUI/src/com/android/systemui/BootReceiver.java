/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.systemui;

import android.content.BroadcastReceiver;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.database.ContentObserver;
import android.os.Handler;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;

/**
 * Performs a number of miscellaneous, non-system-critical actions
 * after the system has finished booting.
 */
public class BootReceiver extends BroadcastReceiver {
    private static final String TAG = "SystemUIBootReceiver";

    private Context mContext;

    @Override
    public void onReceive(final Context context, Intent intent) {
        try {
            // Start the cpu info overlay, if activated
            ContentResolver res = context.getContentResolver();
            if (Settings.Global.getInt(res, Settings.Global.SHOW_CPU_OVERLAY, 0) != 0) {
                Intent cpuinfo = new Intent(context, com.android.systemui.CPUInfoService.class);
                context.startService(cpuinfo);
            }

            // start the screen state service if activated
            if (Settings.System.getIntForUser(mContext.getContentResolver(),
                    Settings.System.START_SCREEN_STATE_SERVICE, 0, UserHandle.USER_CURRENT) != 0) {
                Intent screenstate = new Intent(mContext, com.android.systemui.aosip.screenstate.ScreenStateService.class);
                mContext.startService(screenstate);
            }

        } catch (Exception e) {
            Log.e(TAG, "Can't start cpuinfo service", e);
        }
    }
}
