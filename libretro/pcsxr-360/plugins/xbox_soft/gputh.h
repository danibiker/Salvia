#ifdef __cplusplus
extern "C" {
#endif
	typedef struct s_work{
		void (*func)(void*);
		void * args;
	} work_t;

	int InitThread();
	void AddFunc(void(*func) (void*),void * args);
	 void Core0Free(void * addr);
	 void Core0FreeAll();
	//void AddFunc(funcstuct * func);
	#ifdef __cplusplus
}
#endif