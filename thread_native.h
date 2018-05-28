
extern void rb_native_mutex_lock(rb_nativethread_lock_t *lock);
extern void rb_native_mutex_unlock(rb_nativethread_lock_t *lock);
extern void rb_native_mutex_initialize(rb_nativethread_lock_t *lock);
extern void rb_native_mutex_destroy(rb_nativethread_lock_t *lock);

extern void rb_native_cond_initialize(rb_nativethread_cond_t *cond);
extern void rb_native_cond_destroy(rb_nativethread_cond_t *cond);
extern void rb_native_cond_signal(rb_nativethread_cond_t *cond);
extern void rb_native_cond_broadcast(rb_nativethread_cond_t *cond);
extern void rb_native_cond_wait(rb_nativethread_cond_t *cond, rb_nativethread_lock_t *mutex);
