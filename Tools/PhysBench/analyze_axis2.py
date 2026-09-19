"""축2(비동기 겹침) CSV -> 표 + 그래프.

사용법:
    python Tools/PhysBench/analyze_axis2.py <csv...> [-o <출력 디렉터리>]

동기/비동기의 차이는 프레임 벽시계 시간으로 본다. 겹침 이득의 상한은
min(물리 시간, Actor Tick 시간) 이므로 그 예측치와 실측을 같이 낸다.
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import pandas as pd

matplotlib.rcParams["font.family"] = "Malgun Gothic"
matplotlib.rcParams["axes.unicode_minus"] = False


def load(paths):
    df = pd.concat([pd.read_csv(p) for p in paths], ignore_index=True)

    # sleep 이 끼어들어 활성 바디가 줄어든 구간은 워크로드가 달라진 것이라 제외
    df = df[df["active"] >= df["bodies_cfg"] * 0.95].copy()

    df["physics_ms"] = df["sim_ms"] + df["fetch_ms"]
    return df


def summarize(df):
    # run 단위 중앙값을 먼저 내고, 반복 run 사이의 퍼짐을 따로 본다
    # 프레임 시간은 스파이크에 끌리지 않게 중앙값.
    # 물리·Actor Tick 은 프레임당 평균 — 고정 timestep 이라 물리 스텝이 없는 프레임이
    # 섞이고, 프레임 시간 감소폭을 예측하려면 그 0 프레임까지 포함한 평균이 맞다
    per_run = df.groupby(
        ["bodies_cfg", "threads_cfg", "async", "repeat", "run"]
    ).agg(
        frame_ms=("frame_ms", "median"),
        physics_ms=("physics_ms", "mean"),
        actor_tick_ms=("actor_tick_ms", "mean"),
        substeps=("substeps", "mean"),
        max_substeps=("substeps", "max"),
        frames=("frame_ms", "size"),
    ).reset_index()

    agg = per_run.groupby(["bodies_cfg", "threads_cfg", "async"]).agg(
        runs=("run", "nunique"),
        frame_ms=("frame_ms", "median"),
        frame_min=("frame_ms", "min"),
        frame_max=("frame_ms", "max"),
        physics_ms=("physics_ms", "median"),
        actor_tick_ms=("actor_tick_ms", "median"),
        substeps=("substeps", "median"),
        max_substeps=("max_substeps", "max"),
    ).reset_index()

    sync = agg[agg["async"] == 0].set_index(["bodies_cfg", "threads_cfg"])
    asyn = agg[agg["async"] == 1].set_index(["bodies_cfg", "threads_cfg"])

    out = pd.DataFrame(index=sync.index)
    out["sync_frame_ms"] = sync["frame_ms"]
    out["async_frame_ms"] = asyn["frame_ms"]
    out["gain_ms"] = out["sync_frame_ms"] - out["async_frame_ms"]
    out["gain_pct"] = out["gain_ms"] / out["sync_frame_ms"] * 100.0

    # 겹침 이득의 상한: 동기 모드에서 잰 물리 시간과 Actor Tick 시간 중 작은 쪽
    out["predicted_ms"] = sync[["physics_ms", "actor_tick_ms"]].min(axis=1)
    out["sync_physics_ms"] = sync["physics_ms"]
    out["sync_tick_ms"] = sync["actor_tick_ms"]
    out["max_substeps"] = sync["max_substeps"]

    # 반복 회차 안에서 동기/비동기를 짝지어 이득을 내면, run 간 드리프트가
    # 양쪽에 같이 실리므로 편차를 그대로 비교할 수 있다
    paired = per_run.pivot_table(
        index=["bodies_cfg", "threads_cfg", "repeat"],
        columns="async", values="frame_ms").dropna()
    paired["gain"] = paired[0] - paired[1]

    # 부하가 클수록 회차를 거듭하며 절대 프레임 시간이 올라간다(노트북 전력/발열).
    # 동기·비동기를 같은 회차 안에서 붙여 쟀으므로 드리프트는 양쪽에 같이 실리고,
    # 비율로 보면 상쇄된다 — 비율을 대표값으로 쓴다
    paired["gain_pct"] = paired["gain"] / paired[0] * 100.0

    per_repeat = paired.groupby(["bodies_cfg", "threads_cfg"]).agg(
        gain_rep_min=("gain", "min"),
        gain_rep_max=("gain", "max"),
        gain_pct_median=("gain_pct", "median"),
        gain_pct_min=("gain_pct", "min"),
        gain_pct_max=("gain_pct", "max"),
        repeats=("gain", "size"),
    )
    out = out.join(per_repeat)

    return out.reset_index().sort_values(["threads_cfg", "bodies_cfg"])


def plot_frame_time(summary, out_dir):
    threads = sorted(summary["threads_cfg"].unique())
    fig, axes = plt.subplots(1, len(threads), figsize=(6.2 * len(threads), 4.6), squeeze=False)

    for ax, t in zip(axes[0], threads):
        sub = summary[summary["threads_cfg"] == t].sort_values("bodies_cfg")
        ax.plot(sub["bodies_cfg"], sub["sync_frame_ms"], marker="o", label="동기 (겹침 없음)")
        ax.plot(sub["bodies_cfg"], sub["async_frame_ms"], marker="o", label="비동기 (Actor Tick 과 겹침)")
        ax.fill_between(sub["bodies_cfg"], sub["async_frame_ms"], sub["sync_frame_ms"],
                        alpha=0.18, color="tab:green")

        ax.set_xlabel("dynamic body 수")
        ax.set_ylabel("프레임 시간 (ms, 중앙값)")
        ax.set_title(f"worker threads = {t}")
        ax.grid(True, alpha=0.3)
        ax.legend()

    fig.suptitle("비동기 물리 시뮬레이션 — 프레임 시간 (낮을수록 좋음)")
    fig.tight_layout()
    path = os.path.join(out_dir, "axis2_frame_time.png")
    fig.savefig(path, dpi=140)
    plt.close(fig)
    return path


def plot_gain_vs_prediction(summary, out_dir):
    fig, ax = plt.subplots(figsize=(7.6, 4.8))

    for t, sub in summary.groupby("threads_cfg"):
        sub = sub.sort_values("bodies_cfg")
        line, = ax.plot(sub["bodies_cfg"], sub["gain_ms"], marker="o",
                        label=f"실측 이득 ({t} threads)")
        ax.plot(sub["bodies_cfg"], sub["predicted_ms"], marker="x", linestyle="--",
                color=line.get_color(), alpha=0.65,
                label=f"상한 min(물리, tick) ({t} threads)")

    ax.axhline(0, color="gray", linewidth=0.8)
    ax.set_xlabel("dynamic body 수")
    ax.set_ylabel("프레임 시간 감소 (ms)")
    ax.set_title("겹침 이득 — 실측 vs 상한")
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=8)
    fig.tight_layout()
    path = os.path.join(out_dir, "axis2_gain.png")
    fig.savefig(path, dpi=140)
    plt.close(fig)
    return path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("csv", nargs="+")
    ap.add_argument("-o", "--out-dir", default=".")
    args = ap.parse_args()

    os.makedirs(args.out_dir, exist_ok=True)

    df = load(args.csv)
    summary = summarize(df)
    summary.to_csv(os.path.join(args.out_dir, "axis2_summary.csv"), index=False)

    # MaxSubsteps 상한에 닿으면 물리가 실시간을 따라가지 못해 시뮬레이션한 시간
    # 자체가 모드별로 달라진다. 같은 것을 비교하는 게 아니므로 집계에서 뺀다
    clamped = summary[summary["max_substeps"] >= 8]
    if not clamped.empty:
        print("제외: MaxSubsteps(8) 상한에 닿아 물리가 실시간을 따라가지 못한 설정")
        print(clamped[["bodies_cfg", "threads_cfg", "max_substeps"]].to_string(index=False))
        print()
        summary = summary[summary["max_substeps"] < 8].copy()

    incomplete = summary[summary["repeats"] < summary["repeats"].max()]
    if not incomplete.empty:
        print("주의: 반복 회차가 부족한 설정")
        print(incomplete[["bodies_cfg", "threads_cfg", "repeats"]].to_string(index=False))
        print()

    print(summary.to_string(index=False, float_format=lambda v: f"{v:.2f}"))
    print()

    for p in (plot_frame_time(summary, args.out_dir),
              plot_gain_vs_prediction(summary, args.out_dir)):
        print("wrote", p)


if __name__ == "__main__":
    main()
