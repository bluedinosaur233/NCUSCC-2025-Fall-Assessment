#!/usr/bin/env bash

set -euo pipefail


# 项目根目录执行本脚本

# 目录准备

mkdir -p data

mkdir -p build


# 编译数据生成器（src/gen_data.c -> ./gen_data）

echo "=== 编译 gen_data ==="

gcc -O2 src/gen_data.c -o gen_data


# 生成数据集到 data/

echo "=== 生成测试数据 ==="

./gen_data dataset_1e5.txt 100000

./gen_data dataset_3e5.txt 300000

./gen_data dataset_1e6.txt 1000000


# 输出 CSV

OUT="build/results.csv"

echo "opt,algo,pivot,dataset,size,time_sec,sorted,threads" > "$OUT"


# 设置线程（可根据需要改动或从环境继承）

export OMP_NUM_THREADS="${OMP_NUM_THREADS:-4}"

echo "使用 OMP_NUM_THREADS=$OMP_NUM_THREADS"


# 循环优化等级

for opt in -O0 -O1 -O2 -O3 -Ofast; do

 echo "=== 编译 sort.c with $opt ==="

 gcc $opt -fopenmp src/sort.c -o build/sort


 # 数据集

 for ds in data/dataset_1e5.txt data/dataset_3e5.txt data/dataset_1e6.txt; do

  size=$(wc -l < "$ds")


  # 算法与 pivot

  for algo in quick_rec quick_iter merge_omp; do

   pivots=("na")

   if [[ "$algo" != "merge_omp" ]]; then

    pivots=("rand" "med")

   fi

   for pv in "${pivots[@]}"; do

    echo "运行: $opt $algo $pv $ds"

    line=$(./build/sort "$algo" "$ds" "$pv")


    # 稳健解析（键值对）

    time=$(echo "$line"  | grep -oP 'time_sec=\K[0-9.]+')

    sorted=$(echo "$line" | grep -oP 'sorted=\K[0-9]+')


    # 写入 CSV

    echo "$opt,$algo,$pv,$ds,$size,$time,$sorted,$OMP_NUM_THREADS" >> "$OUT"

   done

  done

 done

done


echo "=== 实验完成，结果已保存到 $OUT ==="
