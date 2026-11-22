#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <string>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "Menu/Menu.hpp"
#include "Menu/Jni.hpp"
#include "Includes/Macros.h"

// ==========================================
// 1. Global Variables for Mods
// ==========================================
bool isDiceAlwaysSix = false;
bool isDevToolEnabled = false;
bool isAdRewardFree = false;

// ==========================================
// 2. Hook Functions
// ==========================================

// --- Dice Hook (RVA: 0x274AF3C) ---
int (*old_Dice_Roll)(void *instance);
int new_Dice_Roll(void *instance) {
    if (instance != NULL && isDiceAlwaysSix) {
        return 6; 
    }
    return old_Dice_Roll(instance);
}

// --- DevTool Hook (RVA: 0x274340C) ---
void (*old_DevTool_Awake)(void *instance);
void new_DevTool_Awake(void *instance) {
    if (instance != NULL) {
        old_DevTool_Awake(instance);
        if (isDevToolEnabled) {
            // Force showDevTool to true (offset 0x20 in DevTool class)
            bool* showDevTool = (bool*)((uint64_t)instance + 0x20); 
            *showDevTool = true;
        }
    }
}

// --- Ad Reward Hook (RVA: 0x275CE90) ---
void (*old_ClaimAdReward)(void *instance, void* success, void* fail);
void new_ClaimAdReward(void *instance, void* success, void* fail) {
    if (instance != NULL) {
        // Just call the original function for now. Server might still reject it.
        old_ClaimAdReward(instance, success, fail);
    }
}
// ==========================================

jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
            OBFUSCATE("Category_Game Mods"),
            OBFUSCATE("Toggle_Always Roll 6"),
            OBFUSCATE("Toggle_Enable DevTool"),
            
            OBFUSCATE("Category_Rewards"),
            OBFUSCATE("Toggle_Claim Ad Reward")
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

void Changes(JNIEnv *env, jclass clazz, jobject obj, jint featNum, jstring featName, jint value, jlong Lvalue, jboolean boolean, jstring text) {
    switch (featNum) {
        case 0: // Category Game Mods
            break;
        case 1: // Toggle Always Roll 6
            isDiceAlwaysSix = boolean;
            break;
        case 2: // Toggle Enable DevTool
            isDevToolEnabled = boolean;
            break;
            
        case 3: // Category Rewards
            break;
        case 4: // Toggle Claim Ad Reward
            isAdRewardFree = boolean;
            break;
        default:
            break;
    }
}

//Target lib here
#define targetLibName OBFUSCATE("libil2cpp.so")

ElfScanner g_il2cppELF;

// we will run our hacks in a new thread so our while loop doesn't block process main thread
void *hack_thread(void *) {
    LOGI(OBFUSCATE("pthread created"));

    do {
        sleep(1);
        g_il2cppELF = ElfScanner::createWithPath(targetLibName);
    } while (!g_il2cppELF.isValid());

    LOGI(OBFUSCATE("%s has been loaded"), (const char *) targetLibName);

#if defined(__aarch64__)
    // Apply Hooks using RVA offsets
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x274AF3C")), new_Dice_Roll, old_Dice_Roll);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x274340C")), new_DevTool_Awake, old_DevTool_Awake);
    HOOK(targetLibName, str2Offset(OBFUSCATE("0x275CE90")), new_ClaimAdReward, old_ClaimAdReward);

#elif defined(__arm__)
    // ARMv7 code if needed
#endif

    LOGI(OBFUSCATE("Hooks Installed Successfully"));
    return nullptr;
}

__attribute__((constructor))
void lib_main() {
    pthread_t ptid;
    pthread_create(&ptid, NULL, hack_thread, NULL);
}
