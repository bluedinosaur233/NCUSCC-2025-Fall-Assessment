import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.font_manager as fm
import os

# ========= 指定中文字体 =========
font_path = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc"
my_font = fm.FontProperties(fname=font_path)

# ========= 输出目录 =========
os.makedirs("report/figs", exist_ok=True)

# ========= 读取数据 =========
df = pd.read_csv("build/results.csv")
df = df[df["sorted"] == 1]  # 只保留排序正确的结果

# ========= 辅助函数 =========
def plot_with_errorbar(data, group_cols, title, filename, xlabel, ylabel):
    plt.figure(figsize=(6,4))
    for key, d in data.groupby(group_cols):
        g = d.groupby("size")["time_sec"].agg(["mean","std"]).reset_index()
        label = key if isinstance(key, str) else "_".join(key)
        plt.errorbar(g["size"], g["mean"], yerr=g["std"], marker="o", capsize=3, label=label)
    plt.xscale("log"); plt.yscale("log")
    plt.xlabel(xlabel, fontproperties=my_font)
    plt.ylabel(ylabel, fontproperties=my_font)
    plt.title(title, fontproperties=my_font)
    plt.legend(prop=my_font)
    plt.tight_layout()
    plt.savefig(filename)
    plt.close()

# ========= 图1：总览（所有算法在 O3 下） =========
sub = df[df["opt"] == "-O3"]
plot_with_errorbar(
    sub, "algo",
    "不同算法在 -O3 下的性能对比",
    "report/figs/all_algos_O3.svg",
    "数据规模 (n)", "运行时间 (秒)"
)

# ========= 图2：每个算法 × 优化等级 =========
for algo in df["algo"].unique():
    sub = df[df["algo"] == algo]
    if sub.empty: continue
    plot_with_errorbar(
        sub, "opt",
        f"{algo} 不同优化等级性能",
        f"report/figs/{algo}_opts.svg",
        "数据规模 (n)", "运行时间 (秒)"
    )

# ========= 图3：每个算法 × 每个优化等级 × pivot =========
for algo in df["algo"].unique():
    for opt in df["opt"].unique():
        sub = df[(df["algo"] == algo) & (df["opt"] == opt)]
        if sub.empty or "pivot" not in sub: continue
        if sub["pivot"].nunique() <= 1: continue
        plot_with_errorbar(
            sub, "pivot",
            f"{algo} 在 {opt} 下不同 pivot 策略",
            f"report/figs/{algo}_{opt}_pivot.svg",
            "数据规模 (n)", "运行时间 (秒)"
        )

print("✅ 全部图表已生成到 report/figs/ 下")
