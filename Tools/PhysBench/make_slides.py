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


C_GUARD = "#eef2f7"
C_GUARD_EDGE = "#8e9bab"


def textbox(ax, x, y, w, h, lines, *, face, edge=C_EDGE, lw=1.2,
            title_size=11.5, body_size=10.2, title_color=C_TEXT,
            body_color=C_MUTED, align="center"):
    ax.add_patch(FancyBboxPatch(
        (x, y), w, h, boxstyle="round,pad=0,rounding_size=0.07",
        facecolor=face, edgecolor=edge, linewidth=lw, zorder=3))

    n = len(lines)
    step = 0.40
    top = y + h / 2 + (n - 1) * step / 2
    cx = x + w / 2 if align == "center" else x + 0.22
    ha = "center" if align == "center" else "left"

    for i, (text, bold) in enumerate(lines):
        ax.text(cx, top - i * step, text, ha=ha, va="center", zorder=4,
                fontsize=title_size if bold else body_size,
                fontweight="bold" if bold else "normal",
                color=title_color if bold else body_color,
                family=MONO if bold and "(" in text else KR)


def arrow(ax, p0, p1, label="", color=C_MUTED, *, lw=1.8, rad=0.0,
          fontsize=10.2, label_offset=(0.0, 0.18), style="-|>"):
    ax.add_patch(FancyArrowPatch(p0, p1, arrowstyle=style, mutation_scale=16,
                                 connectionstyle=f"arc3,rad={rad}",
                                 color=color, linewidth=lw, zorder=5))
    if label:
        ax.text((p0[0] + p1[0]) / 2 + label_offset[0],
                (p0[1] + p1[1]) / 2 + label_offset[1], label,
                ha="center", va="center", fontsize=fontsize,
                color=color, fontweight="bold", zorder=6,
                bbox=dict(boxstyle="round,pad=0.22", facecolor="white",
                          edgecolor="none"))


def architecture(out_dir):
    fig, ax = plt.subplots(figsize=(16.0, 7.8))
    ax.set_xlim(-3.9, 14.6)
    ax.set_ylim(0, 9.2)
    ax.axis("off")

    def row_label(y, title, sub=None):
        ax.text(-3.8, y, title, fontsize=14, fontweight="bold",
                color=C_TEXT, va="center")
        if sub:
            ax.text(-3.8, y - 0.42, sub, fontsize=10.2, color=C_MUTED, va="center")

    ax.text(-3.8, 8.95, "시간 →", fontsize=11.5, color=C_MUTED, va="center")
    ax.annotate("", xy=(13.4, 8.95), xytext=(0.0, 8.95),
                arrowprops=dict(arrowstyle="-|>", color="#c8ced6", linewidth=1.4))

    # ── 1. 게임 스레드 ──────────────────────────────────────────────────
    GY, GH = 7.35, 1.0
    row_label(GY + GH / 2, "게임 스레드", "UWorld::Tick")

    textbox(ax, 0.0, GY, 2.55, GH,
            [("PhysScene->Tick()", True), ("simulate() 호출 후 즉시 반환", False)],
            face="#dce9fa", edge=C_SIM)
    textbox(ax, 2.75, GY, 6.15, GH,
            [("Actor Tick · Lua · Collision", True),
             ("물리가 도는 동안 실행된다", False)],
            face="#daf2e7", edge=C_TICK)
    textbox(ax, 9.10, GY, 4.30, GH,
            [("PhysScene->EndFrame()", True), ("fetchResults(true) 완료 대기", False)],
            face="#fbe8cd", edge=C_FETCH)

    # ── 2. FPhysScene — 경계에서 막는 것 ────────────────────────────────
    row_label(5.50, "FPhysScene", "PhysX 의존성 차단 (PIMPL)")

    guards = [
        (6.10, "scene read lock", "직전 스텝의 pose 를 읽는다"),
        (5.10, "PendingCommands", "쓰기 7종을 큐에 적재"),
        (4.10, "속도 캐시", "캡처해 둔 값을 반환"),
    ]
    for gy, title, sub in guards:
        textbox(ax, 3.30, gy, 5.05, 0.80, [(title, True), (sub, False)],
                face=C_GUARD, edge=C_GUARD_EDGE, lw=1.1,
                title_size=11, body_size=9.8, align="left")

    textbox(ax, 9.10, 4.10, 4.30, 2.80,
            [("결과 확정 후 처리", True),
             ("Transform · Velocity 캡처", False),
             ("렌더 보간 (getActiveActors)", False),
             ("ProcessPendingCommands()", False)],
            face="#fdf4e6", edge=C_FETCH, lw=1.1, align="left")

    # ── 3. PhysX ────────────────────────────────────────────────────────
    PY, PH = 2.35, 0.95
    row_label(PY + PH / 2, "PhysX", "PxScene · worker threads")

    ax.add_patch(Rectangle((0.0, PY), 13.4, PH, facecolor="#f2f4f7",
                           edgecolor="#dfe4ea", linewidth=1, zorder=1))
    block(ax, 1.30, PY + 0.14, 7.60, PH - 0.28, C_SOLVER,
          "solver — worker threads", fontsize=12)

    # ── 연결 ────────────────────────────────────────────────────────────
    arrow(ax, (1.25, GY - 0.05), (1.25, PY + PH + 0.05), "simulate()", C_SIM)
    arrow(ax, (10.20, PY + PH + 0.05), (10.20, 4.05), "시뮬레이션 완료", C_FETCH,
          label_offset=(1.35, 0.0))

    # 읽기는 guard 스택 위로 바로 내려온다
    arrow(ax, (4.30, GY - 0.05), (4.30, 6.95), "읽기", C_GUARD_EDGE, lw=1.6,
          label_offset=(0.62, 0.0))

    # 쓰기는 guard 스택 왼쪽 바깥으로 돌아 PendingCommands 로 들어간다
    ax.add_patch(FancyArrowPatch((2.95, GY - 0.05), (3.25, 5.50),
                                 arrowstyle="-|>", mutation_scale=16,
                                 connectionstyle="angle,angleA=-90,angleB=180,rad=10",
                                 color=C_GUARD_EDGE, linewidth=1.6, zorder=5))
    ax.text(2.62, 6.35, "쓰기", ha="right", va="center", fontsize=10.2,
            color=C_GUARD_EDGE, fontweight="bold", zorder=6)

    arrow(ax, (8.40, 5.50), (9.05, 5.50), "지연 적용", C_GUARD_EDGE,
          lw=1.6, label_offset=(0.0, 0.32))

    # ── 겹침 구간 ───────────────────────────────────────────────────────
    ax.plot([1.30, 1.30, 8.90, 8.90], [PY - 0.22, PY - 0.46, PY - 0.46, PY - 0.22],
            color=C_SOLVER, linewidth=1.5, zorder=4)
    ax.text(5.10, PY - 0.88, "겹침 구간 — 줄어드는 시간의 상한은  min(solver, Actor Tick)",
            ha="center", fontsize=12, color=C_SOLVER, fontweight="bold")

    ax.text(-3.8, 0.40,
            "쓰기는 큐에 쌓였다가 fetchResults() 뒤에 적용되고, 읽기는 직전 스텝 값을 본다  —  "
            "겹치는 동안 시뮬레이션 중인 씬을 직접 건드리지 않는다",
            fontsize=11.5, color=C_MUTED)

    fig.tight_layout()
    return save(fig, out_dir, "slide_architecture")


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

    for p in timeline(args.out_dir) + code(args.out_dir) + architecture(args.out_dir):
        print("wrote", p)


if __name__ == "__main__":
    main()
