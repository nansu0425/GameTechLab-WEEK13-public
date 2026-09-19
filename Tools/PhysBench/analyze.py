"""PhysBench CSV -> 표 + 그래프.

사용법:
    python Tools/PhysBench/analyze.py <csv> [-o <출력 디렉터리>]

축1(스레드 스케일링)은 동기 모드로 측정한다. 프레임당 substep 수가 설정마다
다르므로 모든 물리 시간은 substep 당으로 정규화한 뒤 비교한다.
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

matplotlib.rcParams["font.family"] = "Malgun Gothic"
matplotlib.rcParams["axes.unicode_minus"] = False


BODY_COLORS = {}


def load(path):
    df = pd.read_csv(path)

    # substep 이 0인 프레임은 sim/fetch 가 직전 값을 그대로 들고 있다
    df = df[df["substeps"] > 0].copy()

    # sleep 이 끼어들어 활성 바디가 줄어든 구간은 워크로드가 달라진 것이라 제외
    df = df[df["active"] >= df["bodies_cfg"] * 0.95].copy()

    # 프레임당 substep 수가 설정마다 다르므로 step 당으로 정규화
    df["sim_per_step"] = df["sim_ms"] / df["substeps"]
    df["fetch_per_step"] = df["fetch_ms"] / df["substeps"]
    df["solver_per_step"] = df["sim_per_step"] + df["fetch_per_step"]

    return df


def summarize(df):
    # substep 수가 많은 프레임은 연속 스텝의 캐시 효과로 step 당 비용이 ~8% 낮다.
    # 설정마다 substep 분포가 다르므로, 한 가지 substep 수만 뽑은 값도 같이 낸다
    modal = int(df["substeps"].mode().iloc[0])
    fixed = df[df["substeps"] == modal]

    g = df.groupby(["bodies_cfg", "threads_actual"])
    out = g.agg(
        runs=("run", "nunique"),
        steps=("solver_per_step", "size"),
        solver_ms=("solver_per_step", "median"),
        solver_p25=("solver_per_step", lambda s: s.quantile(0.25)),
        solver_p75=("solver_per_step", lambda s: s.quantile(0.75)),
        actor_tick_ms=("actor_tick_ms", "median"),
        frame_ms=("frame_ms", "median"),
        substeps=("substeps", "median"),
    ).reset_index()

    fixed_med = (fixed.groupby(["bodies_cfg", "threads_actual"])["solver_per_step"]
                 .median().rename(f"solver_ms_sub{modal}"))
    out = out.merge(fixed_med, on=["bodies_cfg", "threads_actual"], how="left")

    # 1 스레드 대비 speedup
    base = (out[out["threads_actual"] == 1]
            .set_index("bodies_cfg")["solver_ms"])
    out["speedup"] = out.apply(
        lambda r: base.get(r["bodies_cfg"], float("nan")) / r["solver_ms"], axis=1)

    return out.sort_values(["bodies_cfg", "threads_actual"])


def plot_solver_time(summary, out_dir):
    fig, ax = plt.subplots(figsize=(7.5, 4.8))

    for bodies, sub in summary.groupby("bodies_cfg"):
        sub = sub.sort_values("threads_actual")
        yerr = [sub["solver_ms"] - sub["solver_p25"], sub["solver_p75"] - sub["solver_ms"]]
        line = ax.errorbar(sub["threads_actual"], sub["solver_ms"], yerr=yerr,
                           marker="o", capsize=3, label=f"{bodies} bodies")
        BODY_COLORS[bodies] = line.lines[0].get_color()

    ax.set_xlabel("PhysX worker threads (0 = 호출 스레드에서 실행)")
    ax.set_ylabel("simulate + fetchResults, substep 당 (ms)")
    ax.set_yscale("log")
    ax.set_title("PhysX solver 시간 vs worker thread 수 (동기 모드, 중앙값 · IQR)")
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()

    path = os.path.join(out_dir, "axis1_solver_time.png")
    fig.savefig(path, dpi=140)
    plt.close(fig)
    return path


def plot_speedup(summary, out_dir):
    fig, ax = plt.subplots(figsize=(7.5, 4.8))

    for bodies, sub in summary.groupby("bodies_cfg"):
        sub = sub[sub["threads_actual"] >= 1].sort_values("threads_actual")
        ax.plot(sub["threads_actual"], sub["speedup"], marker="o",
                color=BODY_COLORS.get(bodies), label=f"{bodies} bodies")

    threads = sorted(t for t in summary["threads_actual"].unique() if t >= 1)
    ax.plot(threads, threads, linestyle="--", color="gray", label="ideal (선형)")

    ax.set_xlabel("PhysX worker threads")
    ax.set_ylabel("1 스레드 대비 speedup")
    ax.set_title("스레드 스케일링 — 실측 vs 선형")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.grid(True, which="both", alpha=0.3)
    ax.legend()
    fig.tight_layout()

    path = os.path.join(out_dir, "axis1_speedup.png")
    fig.savefig(path, dpi=140)
    plt.close(fig)
    return path


def write_markdown(summary, out_dir):
    lines = ["| bodies | threads | solver ms/step | IQR | speedup(vs 1T) | actor tick ms | frame ms | substeps | steps |",
             "|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]

    for _, r in summary.iterrows():
        lines.append(
            f"| {int(r['bodies_cfg'])} | {int(r['threads_actual'])} | {r['solver_ms']:.3f} | "
            f"{r['solver_p25']:.3f}–{r['solver_p75']:.3f} | {r['speedup']:.2f}× | "
            f"{r['actor_tick_ms']:.2f} | {r['frame_ms']:.1f} | {int(r['substeps'])} | {int(r['steps'])} |")

    path = os.path.join(out_dir, "axis1_table.md")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("-o", "--out-dir", default=".")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)

    df = load(args.csv)
    summary = summarize(df)
    summary.to_csv(os.path.join(args.out_dir, "axis1_summary.csv"), index=False)

    print(summary.to_string(index=False, float_format=lambda v: f"{v:.3f}"))
    print()
    for p in (plot_solver_time(summary, args.out_dir),
              plot_speedup(summary, args.out_dir),
              write_markdown(summary, args.out_dir)):
        print("wrote", p)


if __name__ == "__main__":
    main()
