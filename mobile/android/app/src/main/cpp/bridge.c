#include <jni.h>
#include <stdint.h>
#include "mobile_core.h"
#define JNI(name) Java_dev_fofo_maglava_Native_##name
static MLGame *game(jlong handle) { return (MLGame *)(intptr_t)handle; }
JNIEXPORT jlong JNICALL JNI(create)(JNIEnv *e,jobject o) { (void)e;(void)o; return (jlong)(intptr_t)ml_create(); }
JNIEXPORT void JNICALL JNI(destroy)(JNIEnv *e,jobject o,jlong h) { (void)e;(void)o; ml_destroy(game(h)); }
JNIEXPORT void JNICALL JNI(start)(JNIEnv *e,jobject o,jlong h,jint n) { (void)e;(void)o; ml_start(game(h),n); }
JNIEXPORT void JNICALL JNI(pause)(JNIEnv *e,jobject o,jlong h,jboolean p) { (void)e;(void)o; ml_pause(game(h),p); }
JNIEXPORT void JNICALL JNI(input)(JNIEnv *e,jobject o,jlong h,jint c) { (void)e;(void)o; ml_input(game(h),c); }
JNIEXPORT void JNICALL JNI(frame)(JNIEnv *e,jobject o,jlong h,jdouble dt,jfloatArray data) {
    (void)o; if (!h || !data || (*e)->GetArrayLength(e,data)<ML_SNAPSHOT_SIZE) return;
    float snapshot[ML_SNAPSHOT_SIZE]; ml_advance(game(h),dt); ml_snapshot(game(h),snapshot);
    (*e)->SetFloatArrayRegion(e,data,0,ML_SNAPSHOT_SIZE,snapshot);
}
JNIEXPORT jint JNICALL JNI(count)(JNIEnv *e,jobject o) { (void)e;(void)o; return ml_level_count(); }
JNIEXPORT jstring JNICALL JNI(metadata)(JNIEnv *e,jobject o,jint n,jint field) {
    (void)o; const char *s=field==0?ml_level_key(n):field==1?ml_level_name(n):ml_level_hint(n);
    return (*e)->NewStringUTF(e,s);
}

#include "presentation.h"
#include <string.h>
JNIEXPORT jlong JNICALL JNI(sceneCreate)(JNIEnv *e,jobject o) { (void)e;(void)o;return (jlong)(intptr_t)ml_scene_create(); }
JNIEXPORT void JNICALL JNI(sceneDestroy)(JNIEnv *e,jobject o,jlong h) { (void)e;(void)o;ml_scene_destroy((MLScene*)(intptr_t)h); }
JNIEXPORT void JNICALL JNI(sceneBuild)(JNIEnv *e,jobject o,jlong h,jfloatArray data,jfloat aspect,jboolean reduced,jobject buffer,jintArray counts) {
    (void)o;
    if(!h||(*e)->GetArrayLength(e,data)<ML_SNAPSHOT_SIZE||(*e)->GetArrayLength(e,counts)<2)return;
    MLScene *scene=(MLScene*)(intptr_t)h;
    float s[ML_SNAPSHOT_SIZE];(*e)->GetFloatArrayRegion(e,data,0,ML_SNAPSHOT_SIZE,s);
    ml_scene_build(scene,s,aspect,reduced);
    jint n[2]={ml_scene_opaque_count(scene),ml_scene_vertex_count(scene)};
    void *out=(*e)->GetDirectBufferAddress(e,buffer);
    if(!out||(*e)->GetDirectBufferCapacity(e,buffer)<(jlong)(n[1]*sizeof(MLVertex)))return;
    memcpy(out,ml_scene_vertices(scene),n[1]*sizeof(MLVertex));(*e)->SetIntArrayRegion(e,counts,0,2,n);
}
JNIEXPORT void JNICALL JNI(controlLayout)(JNIEnv *e,jobject o,jfloat w,jfloat h,jfloatArray out) {
    (void)o;if((*e)->GetArrayLength(e,out)<16)return;float r[16];ml_control_layout(w,h,r);(*e)->SetFloatArrayRegion(e,out,0,16,r);
}
JNIEXPORT jstring JNICALL JNI(musicTrack)(JNIEnv *e,jobject o,jint n) { (void)o;return (*e)->NewStringUTF(e,ml_music_track(n)); }

JNIEXPORT jint JNICALL JNI(magnetColor)(JNIEnv *e,jobject o,jint c) { (void)e;(void)o;return ml_magnet_color(c); }
JNIEXPORT jint JNICALL JNI(accentColor)(JNIEnv *e,jobject o,jint n) { (void)e;(void)o;return ml_accent_color(n); }
JNIEXPORT jstring JNICALL JNI(chapterName)(JNIEnv *e,jobject o,jint n) { (void)o;return (*e)->NewStringUTF(e,ml_chapter_name(n)); }

JNIEXPORT jint JNICALL JNI(musicCount)(JNIEnv *e,jobject o) { (void)e;(void)o;return ml_music_count(); }
JNIEXPORT void JNICALL JNI(menuFrame)(JNIEnv *e,jobject o,jfloat time,jfloat aspect,jboolean reduced,jfloatArray out) {
    (void)o;if((*e)->GetArrayLength(e,out)<ML_SNAPSHOT_SIZE)return;
    float s[ML_SNAPSHOT_SIZE];ml_menu_snapshot(time,aspect,reduced,s);
    (*e)->SetFloatArrayRegion(e,out,0,ML_SNAPSHOT_SIZE,s);
}
