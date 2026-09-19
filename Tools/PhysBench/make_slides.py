"""포트폴리오 장표용 그림 생성.

    python Tools/PhysBench/make_slides.py -o <출력 디렉터리>

두 장을 만든다.
  slide_timeline.(png|svg)  한 프레임의 실행 순서 — 동기 vs 비동기
  slide_code.(png|svg)      UWorld::Tick 변경 전/후 (커밋 0e1ad224)

두 그림은 같은 번호 ①②③④ 를 쓴다. 코드의 어느 줄이 타임라인의 어느 블록인지
장표에서 바로 대응시키기 위해서다.
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, FancyBboxPatch, Rectangle

KR = "Malgun Gothic"
MONO = "Consolas"

C_EDGE = "#2b3440"
C_TEXT = "#1b2430"
C_MUTED = "#6b7785"
C_START = "#b9c2cc"      # StartFrame / EndFrame 래퍼
C_SIM = "#3b7dd8"        # simulate()
C_WAIT = "#d94f4f"       # fetchResults 블로킹 대기
C_FETCH = "#e8a33d"      # fetchResults 실제 수집
C_TICK = "#3faf7d"       # Actor Tick 구간
C_SOLVER = "#8a63d2"     # PhysX worker solver
C_GAIN = "#1f9d55"

matplotlib.rcParams["font.family"] = KR
matplotlib.rcParams["axes.unicode_minus"] = False


def block(ax, x, y, w, h, color, label, *, hatch=None, fontsize=11, tc="white"):
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h,
        boxstyle="round,pad=0,rounding_size=0.06",
        facecolor=color, edgecolor=C_EDGE, linewidth=1.1, hatch=hatch, zorder=3))
    if label:
        ax.text(x + w / 2, y + h / 2, label, ha="center", va="center",
                color=tc, fontsize=fontsize, fontweight="bold", zorder=4)


def lane(ax, x0, x1, y, h, name):
    ax.add_patch(Rectangle((x0, y), x1 - x0, h, facecolor="#f2f4f7",
                           edgecolor="#dfe4ea", linewidth=1, zorder=1))
    ax.text(x0 - 0.25, y + h / 2, name, ha="right", va="center",
            fontsize=11.5, color=C_TEXT)


def timeline(out_dir):
    """장표 상단 배너. 전달할 것은 하나 — fetchResults() 가 어디에 있는가."""
    fig, ax = plt.subplots(figsize=(16.0, 4.6))
    ax.set_xlim(-3.6, 10.2)
    ax.set_ylim(0, 6.3)
    ax.axis("off")

    LH, BH = 0.72, 0.58
    X_END = 9.9

    W_SIM = 0.85          # simulate() — 비블로킹이라 짧다
    W_SOLVER = 3.54       # PhysX worker 가 실제로 푸는 시간
    W_TICK = 4.89         # Actor Tick · Lua · Collision
    W_FETCH = 1.30        # 비동기에서 남은 대기 + 결과 수집

    X_SOLVER = W_SIM + 0.03
    SYNC_FRAME = X_SOLVER + W_SOLVER + 0.03 + W_TICK
    ASYNC_FRAME = X_SOLVER + W_TICK + 0.03 + W_FETCH

    def row(gy, wy, title):
        ax.text(-3.5, (gy + wy + LH) / 2, title, fontsize=15,
                fontweight="bold", color=C_TEXT, va="center")
        lane(ax, 0, X_END, gy, LH, "게임 스레드")
        lane(ax, 0, X_END, wy, LH, "PhysX worker")
        return gy + (LH - BH) / 2, wy + (LH - BH) / 2

    # ── 동기 — fetchResults() 가 Actor Tick 앞에 있다 ───────────────────
    gy, wy = 4.95, 4.05
    by, bw = row(gy, wy, "동기")

    block(ax, 0.00, by, W_SIM, BH, C_SIM, "simulate()", fontsize=9)
    block(ax, X_SOLVER, by, W_SOLVER, BH, C_WAIT,
          "fetchResults() 블로킹 대기", hatch="//", fontsize=11.5)
    block(ax, X_SOLVER + W_SOLVER + 0.03, by, W_TICK, BH, C_TICK,
          "Actor Tick · Lua · Collision", fontsize=11.5)
    block(ax, X_SOLVER, bw, W_SOLVER, BH, C_SOLVER, "solver", fontsize=11.5)

    ax.annotate("게임 스레드가 논다",
                xy=(X_SOLVER + W_SOLVER / 2, wy - 0.02),
                xytext=(X_SOLVER + W_SOLVER / 2, wy - 0.62),
                ha="center", fontsize=12, color=C_WAIT, fontweight="bold",
                arrowprops=dict(arrowstyle="-|>", color=C_WAIT, linewidth=1.7))

    # ── 비동기 — fetchResults() 를 Actor Tick 뒤로 보낸다 ───────────────
    gy2, wy2 = 2.30, 1.40
    by2, bw2 = row(gy2, wy2, "비동기")

    block(ax, 0.00, by2, W_SIM, BH, C_SIM, "simulate()", fontsize=9)
    block(ax, X_SOLVER, by2, W_TICK, BH, C_TICK,
          "Actor Tick · Lua · Collision", fontsize=11.5)
    block(ax, X_SOLVER + W_TICK + 0.03, by2, W_FETCH, BH, C_FETCH,
          "fetchResults()", fontsize=9.5)
    block(ax, X_SOLVER, bw2, W_SOLVER, BH, C_SOLVER, "solver", fontsize=11.5)

    ax.annotate("겹친다",
                xy=(X_SOLVER + W_SOLVER / 2, wy2 - 0.02),
                xytext=(X_SOLVER + W_SOLVER / 2, wy2 - 0.62),
                ha="center", fontsize=12, color=C_SOLVER, fontweight="bold",
                arrowprops=dict(arrowstyle="-|>", color=C_SOLVER, linewidth=1.7))

    # ── 프레임 끝 · 절감 구간 ───────────────────────────────────────────
    for x, y_top in ((SYNC_FRAME, gy + LH + 0.02), (ASYNC_FRAME, gy2 + LH + 0.02)):
        ax.plot([x, x], [0.78, y_top], color=C_MUTED,
                linestyle=(0, (4, 3)), linewidth=1.3, zorder=2)
        ax.text(x + 0.10, y_top + 0.08, "프레임 끝", fontsize=10.5, color=C_MUTED)

    ax.add_patch(FancyArrowPatch((ASYNC_FRAME, 0.95), (SYNC_FRAME, 0.95),
                                 arrowstyle="<|-|>", mutation_scale=13,
                                 color=C_GAIN, linewidth=2.0, zorder=5))
    ax.text((ASYNC_FRAME + SYNC_FRAME) / 2, 0.48, "줄어든 프레임 시간",
            ha="center", fontsize=12, color=C_GAIN, fontweight="bold")

    fig.tight_layout()
    return save(fig, out_dir, "slide_timeline")


CODE_BEFORE = [
    ("// 물리 시뮬레이션 (Actor Tick 전에 실행)", None),
    ("if (PhysScene && PhysScene->IsInitialized())", None),
    ("{", None),
    ("    PhysScene->StartFrame();", "start"),
    ("    PhysScene->Tick(GetDeltaTime(EDeltaTime::Game));", "sim"),
    ("    PhysScene->EndFrame();", "move"),
    ("}", None),
    ("", None),
    ("if (Level)", None),
    ("{", None),
    ("    for (AActor* Actor : LevelActors)", "tick"),
    ("        Actor->Tick(...);", "tick"),
    ("}", None),
]

CODE_AFTER = [
    ("// 1. Pre-simulation 작업", None),
    ("if (PhysScene && PhysScene->IsInitialized())", None),
    ("{", None),
    ("    PhysScene->StartFrame();", "start"),
    ("}", None),
    ("", None),
    ("// 2. 시뮬레이션 시작 (비블로킹)", None),
    ("if (PhysScene && PhysScene->IsInitialized())", None),
    ("{", None),
    ("    PhysScene->Tick(GetDeltaTime(EDeltaTime::Game));", "sim"),
    ("}", None),
    ("", None),
    ("// 3. Actor Tick (물리 시뮬레이션과 병렬 실행)", None),
    ("if (Level)", None),
    ("{", None),
    ("    for (AActor* Actor : LevelActors)", "tick"),
    ("        Actor->Tick(...);", "tick"),
    ("}", None),
    ("", None),
    ("// 4. 렌더링 전 물리 결과 수집", None),
    ("if (PhysScene && PhysScene->IsInitialized())", None),
    ("{", None),
    ("    PhysScene->EndFrame();", "move"),
    ("}", None),
]

HL = {"start": "#eceff3", "sim": "#dce9fa", "tick": "#daf2e7", "move": "#fde0dd"}
HL_AFTER_MOVE = "#d6f2e0"


def code_panel(ax, title, subtitle, lines, x0, width, move_color):
    ax.text(x0, 0.965, title, fontsize=15, fontweight="bold",
            color=C_TEXT, transform=ax.transAxes)
    ax.text(x0, 0.928, subtitle, fontsize=11.5, color=C_MUTED,
            transform=ax.transAxes)

    top, lh = 0.875, 0.0335
    ax.add_patch(Rectangle((x0 - 0.012, top - lh * len(lines) - 0.022),
                           width, lh * len(lines) + 0.042,
                           facecolor="#fbfcfd", edgecolor="#dfe4ea",
                           linewidth=1.2, transform=ax.transAxes, zorder=1))

    for i, (text, kind) in enumerate(lines):
        y = top - i * lh
        if kind:
            color = move_color if kind == "move" else HL[kind]
            ax.add_patch(Rectangle((x0 - 0.006, y - 0.010), width - 0.012, lh - 0.004,
                                   facecolor=color, edgecolor="none",
                                   transform=ax.transAxes, zorder=2))
        is_comment = text.strip().startswith("//")
        ax.text(x0, y, text, fontsize=11.4, family=MONO if not is_comment else KR,
                color=C_MUTED if is_comment else C_TEXT,
                va="center", transform=ax.transAxes, zorder=3)


def code(out_dir):
    fig, ax = plt.subplots(figsize=(15.5, 8.2))
    ax.axis("off")

    code_panel(ax, "변경 전 — 동기",
               "물리가 끝나야 Actor Tick 이 시작된다",
               CODE_BEFORE, 0.035, 0.435, HL["move"])

    code_panel(ax, "변경 후 — 비동기",
               "EndFrame() 한 줄을 Actor Tick 뒤로 옮겼다",
               CODE_AFTER, 0.535, 0.435, HL_AFTER_MOVE)

    # 화살표는 두 패널 사이 여백으로만 지나가게 한다 — 코드를 가리면 안 된다
    y_from = 0.875 - 5 * 0.0335          # 변경 전의 EndFrame() 줄
    y_to = 0.875 - 22 * 0.0335           # 변경 후의 EndFrame() 줄
    ax.add_patch(FancyArrowPatch((0.476, y_from), (0.521, y_to),
                                 transform=ax.transAxes,
                                 arrowstyle="-|>", mutation_scale=22,
                                 connectionstyle="arc3,rad=0.06",
                                 color=C_GAIN, linewidth=2.4, zorder=5))
    ax.text(0.4985, (y_from + y_to) / 2, "이동", ha="center", va="center",
            fontsize=12.5, color=C_GAIN, fontweight="bold", transform=ax.transAxes,
            zorder=6,
            bbox=dict(boxstyle="round,pad=0.3", facecolor="white",
                      edgecolor=C_GAIN, linewidth=1.2))

    ax.text(0.035, 0.055,
            "커밋 0e1ad224 — UWorld::Tick.  이 한 줄의 위치가 바뀐 것이 비동기 전환의 전부다.",
            fontsize=12.2, color=C_MUTED, transform=ax.transAxes)

    fig.tight_layout()
    return save(fig, out_dir, "slide_code")


def save(fig, out_dir, name):
    paths = []
    for ext, kw in (("png", dict(dpi=200)), ("svg", {})):
        p = os.path.join(out_dir, f"{name}.{ext}")
        fig.savefig(p, facecolor="white", **kw)
        paths.append(p)
    plt.close(fig)
    return paths


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--out-dir", default=".")
    args = ap.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)

    for p in timeline(args.out_dir) + code(args.out_dir):
        print("wrote", p)


if __name__ == "__main__":
    main()
