#include <stdio.h>

#include <stdlib.h>

#include <time.h>

#include <string.h>

#include <sys/stat.h>

#include <sys/types.h>


int main(int argc, char* argv[]) {

  if (argc < 3) {

    printf("用法: %s <文件名> <数据量>\n", argv[0]);

    printf("示例: %s dataset_1e5.txt 100000\n", argv[0]);

    return 1;

  }


  const char* filename = argv[1];

  long n = atol(argv[2]);


  // 确保 data 目录存在

  struct stat st = {0};

  if (stat("data", &st) == -1) {

    mkdir("data", 0755);

  }


  // 拼接路径 data/filename

  char path[256];

  snprintf(path, sizeof(path), "data/%s", filename);


  FILE* f = fopen(path, "w");

  if (!f) {

    perror("打开文件失败");

    return 1;

  }


  srand((unsigned)time(NULL));


  for (long i = 0; i < n; i++) {

    long val = ((long)rand() << 32 | rand()) % 2000000001L - 1000000000L;

    fprintf(f, "%ld\n", val);

  }


  fclose(f);

  printf("已生成 %ld 条数据到 %s\n", n, path);

  return 0;

}


