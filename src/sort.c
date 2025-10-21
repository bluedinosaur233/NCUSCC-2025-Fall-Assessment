#include <stdio.h>

#include <stdlib.h>

#include <stdint.h>

#include <string.h>

#include <time.h>

#include <errno.h>

#ifdef _OPENMP

#include <omp.h>

#endif


typedef long T; // 在 Linux x86_64 上 long 是 64 位，对应 %ld


// ========== 工具函数 ==========

static inline void swap(T* a, T* b) { T tmp = *a; *a = *b; *b = tmp; }


static inline double now_sec() {

 struct timespec ts;

 clock_gettime(CLOCK_MONOTONIC, &ts);

 return ts.tv_sec + ts.tv_nsec / 1e9;

}


static inline long read_data_file(const char* path, T** out) {

 FILE* f = fopen(path, "r");

 if (!f) { perror("open"); return -1; }

 long cap = 1024, n = 0;

 T* a = (T*)malloc(cap * sizeof(T));

 if (!a) { perror("malloc"); fclose(f); return -1; }


 while (1) {

  T v;

  int r = fscanf(f, "%ld", &v);

  if (r == 1) {

   if (n == cap) {

    cap *= 2;

    T* na = (T*)realloc(a, cap * sizeof(T));

    if (!na) { perror("realloc"); free(a); fclose(f); return -1; }

    a = na;

   }

   a[n++] = v;

  } else if (r == EOF) {

   break;

  } else {

   // 跳过异常行

   int c;

   while ((c = fgetc(f)) != '\n' && c != EOF) {}

  }

 }

 fclose(f);

 *out = a;

 return n;

}


static inline int is_sorted(const T* a, long n) {

 for (long i = 1; i < n; i++) if (a[i-1] > a[i]) return 0;

 return 1;

}


// ========== 快速排序 ==========

typedef enum { PIVOT_RANDOM=0, PIVOT_MEDIAN3=1, PIVOT_LAST=2 } pivot_t;


static inline long median3_index(T* arr, long l, long m, long r) {

 T a = arr[l], b = arr[m], c = arr[r];

 if ((a <= b && b <= c) || (c <= b && b <= a)) return m;

 if ((b <= a && a <= c) || (c <= a && a <= b)) return l;

 return r;

}


static inline long choose_pivot(T* arr, long l, long r, pivot_t pt) {

 if (pt == PIVOT_LAST) return r;

 if (pt == PIVOT_RANDOM) return l + rand() % (r - l + 1);

 long m = l + (r - l) / 2;

 return median3_index(arr, l, m, r);

}


static long partition(T* arr, long l, long r, pivot_t pt) {

 long p = choose_pivot(arr, l, r, pt);

 swap(&arr[p], &arr[r]);

 T pivot = arr[r];

 long i = l;

 for (long j = l; j < r; j++) {

  if (arr[j] <= pivot) { swap(&arr[i], &arr[j]); i++; }

 }

 swap(&arr[i], &arr[r]);

 return i;

}


// 递归版（尾递归消除）

void quicksort_recursive(T* arr, long l, long r, pivot_t pt) {

 while (l < r) {

  long p = partition(arr, l, r, pt);

  if (p - l < r - p) {

   if (l < p) quicksort_recursive(arr, l, p - 1, pt);

   l = p + 1;

  } else {

   if (p + 1 < r) quicksort_recursive(arr, p + 1, r, pt);

   r = p - 1;

  }

 }

}


// 非递归版

void quicksort_iterative(T* arr, long n, pivot_t pt) {

 typedef struct { long l, r; } Range;

 Range* stack = (Range*)malloc((n > 0 ? n : 1) * sizeof(Range));

 if (!stack) { perror("malloc"); return; }

 long top = 0;

 stack[top++] = (Range){0, n - 1};


 while (top) {

  Range cur = stack[--top];

  long l = cur.l, r = cur.r;

  while (l < r) {

   long p = partition(arr, l, r, pt);

   if (p - l < r - p) {

    if (p + 1 <= r) stack[top++] = (Range){p + 1, r};

    r = p - 1;

   } else {

    if (l <= p - 1) stack[top++] = (Range){l, p - 1};

    l = p + 1;

   }

  }

 }

 free(stack);

}


// ========== 并行归并排序 ==========

static void insertion_sort(T* arr, long l, long r) {

 for (long i = l + 1; i <= r; i++) {

  T key = arr[i]; long j = i - 1;

  while (j >= l && arr[j] > key) { arr[j+1] = arr[j]; j--; }

  arr[j+1] = key;

 }

}


static void merge(T* arr, T* tmp, long l, long m, long r) {

 long i=l, j=m+1, k=l;

 while (i<=m && j<=r) tmp[k++] = (arr[i]<=arr[j]) ? arr[i++] : arr[j++];

 while (i<=m) tmp[k++] = arr[i++];

 while (j<=r) tmp[k++] = arr[j++];

 for (long x=l; x<=r; x++) arr[x] = tmp[x];

}


static void mergesort_seq(T* arr, T* tmp, long l, long r) {

 if (r - l <= 32) { insertion_sort(arr, l, r); return; }

 long m = l + (r - l) / 2;

 mergesort_seq(arr, tmp, l, m);

 mergesort_seq(arr, tmp, m + 1, r);

 merge(arr, tmp, l, m, r);

}


static void mergesort_parallel_internal(T* arr, T* tmp, long l, long r, int depth) {

 if (r - l <= 32) { insertion_sort(arr, l, r); return; }

 long m = l + (r - l) / 2;

 if (depth > 0) {

  #pragma omp task shared(arr, tmp)

  mergesort_parallel_internal(arr, tmp, l, m, depth - 1);

  #pragma omp task shared(arr, tmp)

  mergesort_parallel_internal(arr, tmp, m + 1, r, depth - 1);

  #pragma omp taskwait

 } else {

  mergesort_seq(arr, tmp, l, m);

  mergesort_seq(arr, tmp, m + 1, r);

 }

 merge(arr, tmp, l, m, r);

}


void mergesort_parallel(T* arr, long n) {

 T* tmp = (T*)malloc(n * sizeof(T));

 if (!tmp) { perror("malloc"); return; }

 int max_depth = 0;

 #ifdef _OPENMP

 int threads = omp_get_max_threads();

 while ((1 << max_depth) < threads) max_depth++;

 #endif

 #pragma omp parallel

 {

  #pragma omp single

  mergesort_parallel_internal(arr, tmp, 0, n - 1, max_depth);

 }

 free(tmp);

}


// ========== 主函数 ==========

static pivot_t parse_pivot(const char* s) {

 if (strcmp(s, "rand") == 0) return PIVOT_RANDOM;

 if (strcmp(s, "med") == 0) return PIVOT_MEDIAN3;

 if (strcmp(s, "last") == 0) return PIVOT_LAST;

 // 默认用 median-of-three

 return PIVOT_MEDIAN3;

}


int main(int argc, char** argv) {

 if (argc < 3) {

  fprintf(stderr, "用法: %s <algo> <input_file> [pivot]\n", argv[0]);

  fprintf(stderr, "algo: quick_rec | quick_iter | merge_omp\n");

  fprintf(stderr, "pivot: rand | med | last （仅 quick_* 有效，默认 med）\n");

  return 1;

 }

 const char* algo = argv[1];

 const char* input = argv[2];

 const char* pivot_str = (argc >= 4 ? argv[3] : "med");

 pivot_t pt = parse_pivot(pivot_str);


 // 读取数据

 T* arr = NULL;

 long n = read_data_file(input, &arr);

 if (n <= 0 || arr == NULL) { fprintf(stderr, "读取失败\n"); return 1; }


 // 设置随机种子（用于 rand pivot）

 srand((unsigned)time(NULL));


 double t0 = now_sec();

 if (strcmp(algo, "quick_rec") == 0) {

  quicksort_recursive(arr, 0, n - 1, pt);

 } else if (strcmp(algo, "quick_iter") == 0) {

  quicksort_iterative(arr, n, pt);

 } else if (strcmp(algo, "merge_omp") == 0) {

  mergesort_parallel(arr, n);

  // merge_omp 忽略 pivot

  pivot_str = "na";

 } else {

  fprintf(stderr, "未知算法: %s\n", algo);

  free(arr);

  return 1;

 }

 double t1 = now_sec();


 int ok = is_sorted(arr, n);

 // 机器友好输出：键值对，便于脚本解析

 printf("algo=%s,pivot=%s,n=%ld,time_sec=%.6f,sorted=%d\n",

     algo, pivot_str, n, t1 - t0, ok);


 free(arr);

 return 0;

}
