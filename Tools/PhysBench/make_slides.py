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
    fig, ax = plt.subplots(figsize=(15.5, 8.0))
    ax.set_xlim(-3.4, 13.2)
    ax.set_ylim(0, 11.4)
    ax.axis("off")

    LH = 0.78
    BH = 0.62
    X_SYNC_END = 11.55
    X_ASYNC_END = 8.20

    # ── 동기 ────────────────────────────────────────────────────────────
    gy, wy = 8.65, 7.65
    ax.text(-3.3, 10.72, "동기 — simulate() 와 fetchResults() 를 붙여서 호출",
            fontsize=14.5, fontweight="bold", color=C_TEXT)

    lane(ax, 0, 11.6, gy, LH, "게임 스레드")
    lane(ax, 0, 11.6, wy, LH, "PhysX worker")

    by = gy + (LH - BH) / 2
    block(ax, 0.05, by, 0.75, BH, C_START, "①", fontsize=12)
    block(ax, 0.85, by, 0.75, BH, C_SIM, "②", fontsize=12)
    block(ax, 1.65, by, 3.6, BH, C_WAIT, "fetchResults() 블로킹 대기", hatch="//")
    block(ax, 5.30, by, 5.5, BH, C_TICK, "③  Actor Tick · Lua · Collision")
    block(ax, 10.85, by, 0.7, BH, C_START, "", fontsize=12)

    block(ax, 1.65, wy + (LH - BH) / 2, 3.6, BH, C_SOLVER, "solver")

    ax.annotate("게임 스레드가 노는 구간",
                xy=(3.45, gy + LH + 0.05), xytext=(3.45, 10.15),
                ha="center", fontsize=12.5, color=C_WAIT, fontweight="bold",
                arrowprops=dict(arrowstyle="-|>", color=C_WAIT, linewidth=1.8))

    ax.plot([X_SYNC_END, X_SYNC_END], [wy - 0.2, gy + LH + 0.45], color=C_MUTED,
            linestyle=(0, (4, 3)), linewidth=1.4, zorder=2)
    ax.text(X_SYNC_END + 0.12, gy + LH + 0.5, "프레임 끝", fontsize=11, color=C_MUTED)

    # ── 비동기 ──────────────────────────────────────────────────────────
    gy2, wy2 = 4.35, 3.35
    ax.text(-3.3, 6.42, "비동기 — 두 호출 사이에 Actor Tick 을 끼워 넣는다",
            fontsize=14.5, fontweight="bold", color=C_TEXT)

    lane(ax, 0, 11.6, gy2, LH, "게임 스레드")
    lane(ax, 0, 11.6, wy2, LH, "PhysX worker")

    by2 = gy2 + (LH - BH) / 2
    block(ax, 0.05, by2, 0.75, BH, C_START, "①", fontsize=12)
    block(ax, 0.85, by2, 0.75, BH, C_SIM, "②", fontsize=12)
    block(ax, 1.65, by2, 5.5, BH, C_TICK, "③  Actor Tick · Lua · Collision")
    block(ax, 7.20, by2, 1.0, BH, C_FETCH, "④", fontsize=12)

    block(ax, 1.65, wy2 + (LH - BH) / 2, 3.6, BH, C_SOLVER, "solver")

    ax.annotate("겹친다", xy=(3.45, wy2 - 0.02), xytext=(3.45, 2.42),
                ha="center", fontsize=12.5, color=C_SOLVER, fontweight="bold",
                arrowprops=dict(arrowstyle="-|>", color=C_SOLVER, linewidth=1.8))

    for x, label in ((X_ASYNC_END, "프레임 끝"), (X_SYNC_END, "동기였다면")):
        ax.plot([x, x], [1.72, gy2 + LH + 0.45], color=C_MUTED,
                linestyle=(0, (4, 3)), linewidth=1.4, zorder=2)
        ax.text(x + 0.12, gy2 + LH + 0.5, label, fontsize=11, color=C_MUTED)

    # ── 절감 구간 ───────────────────────────────────────────────────────
    ax.add_patch(FancyArrowPatch((X_ASYNC_END, 1.95), (X_SYNC_END, 1.95),
                                 arrowstyle="<|-|>", mutation_scale=15,
                                 color=C_GAIN, linewidth=2.1, zorder=5))
    ax.text((X_ASYNC_END + X_SYNC_END) / 2, 1.5, "줄어든 프레임 시간",
            ha="center", fontsize=12.5, color=C_GAIN, fontweight="bold")

    ax.text(-3.3, 0.95,
            "줄일 수 있는 시간의 상한은  min(물리 계산 시간, ③ 구간 비용)  —  둘 중 짧은 쪽이 끝나면 겹칠 것이 없다",
            fontsize=12.2, color=C_MUTED)

    # ── 범례 ────────────────────────────────────────────────────────────
    legend = [
        (C_START, "① StartFrame / EndFrame"),
        (C_SIM, "② simulate()  비블로킹"),
        (C_TICK, "③ 게임 스레드 작업"),
        (C_FETCH, "④ fetchResults()"),
        (C_SOLVER, "PhysX worker solver"),
    ]
    lx = -3.3
    for color, text in legend:
        ax.add_patch(Rectangle((lx, 0.18), 0.3, 0.26, facecolor=color,
                               edgecolor=C_EDGE, linewidth=0.9))
        ax.text(lx + 0.42, 0.31, text, va="center", fontsize=10.8, color=C_TEXT)
        lx += 0.42 + len(text) * 0.155 + 0.5

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
