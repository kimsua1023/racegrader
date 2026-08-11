package tui

import (
	"context"
	"fmt"
	"path/filepath"
	"strings"
	"time"

	tea "charm.land/bubbletea/v2"

	"racegrader/cli/internal/report"
	"racegrader/cli/internal/run"
)

type phase int

const (
	phaseSplash phase = iota
	phaseBoot
	phaseRunning
	phaseDone
	phaseError
)

const (
	maxLogLines = 10
	tickEvery   = 140 * time.Millisecond
	splashTicks = 22
)

type model struct {
	cfg run.Config

	phase     phase
	finished  int
	failures  int
	timeouts  int
	passes    int
	tick      int
	width     int
	height    int
	err       error
	log       []run.Result
	results   []run.Result
	fails     []run.Result
	started   time.Time
	elapsed   time.Duration
	passed    bool
	reportOK  bool
	reportErr error

	ctx           context.Context
	cancel        context.CancelFunc
	eventCh       chan run.Event
	runnerStarted bool
}

type tickMsg time.Time
type sessionEventMsg run.Event

func Start(cfg run.Config) error {
	ctx, cancel := context.WithCancel(context.Background())
	events := make(chan run.Event, 64)

	m := model{
		cfg:     cfg,
		width:   80,
		height:  24,
		ctx:     ctx,
		cancel:  cancel,
		eventCh: events,
		log:     make([]run.Result, 0, maxLogLines),
		results: make([]run.Result, 0, cfg.Repeat),
		fails:   make([]run.Result, 0, 8),
	}
	if cfg.SkipBoot {
		m.phase = phaseBoot
		m.startRunner()
	} else {
		m.phase = phaseSplash
	}

	_, err := tea.NewProgram(m).Run()
	cancel()
	return err
}

func (m *model) startRunner() {
	if m.runnerStarted {
		return
	}
	m.runnerStarted = true
	go run.NewRunner(m.cfg).Run(m.ctx, m.eventCh)
}

func (m model) Init() tea.Cmd {
	if m.runnerStarted {
		return tea.Batch(waitEvent(m.eventCh), tickCmd())
	}
	return tickCmd()
}

func tickCmd() tea.Cmd {
	return tea.Tick(tickEvery, func(t time.Time) tea.Msg { return tickMsg(t) })
}

func waitEvent(ch <-chan run.Event) tea.Cmd {
	return func() tea.Msg {
		ev, ok := <-ch
		if !ok {
			return sessionEventMsg{Kind: run.EventDone}
		}
		return sessionEventMsg(ev)
	}
}

func (m model) Update(msg tea.Msg) (tea.Model, tea.Cmd) {
	switch msg := msg.(type) {
	case tea.WindowSizeMsg:
		m.width, m.height = msg.Width, msg.Height
		return m, nil

	case tea.KeyPressMsg:
		if msg.String() == "ctrl+c" {
			if m.cancel != nil {
				m.cancel()
			}
			return m, tea.Quit
		}

	case tickMsg:
		m.tick++
		var cmds []tea.Cmd
		if m.phase == phaseSplash && m.tick >= splashTicks {
			m.phase = phaseBoot
			m.startRunner()
			cmds = append(cmds, waitEvent(m.eventCh))
		}
		if m.phase != phaseDone && m.phase != phaseError {
			cmds = append(cmds, tickCmd())
		}
		return m, tea.Batch(cmds...)

	case sessionEventMsg:
		return m.applyEvent(run.Event(msg))
	}
	return m, nil
}

func (m model) applyEvent(ev run.Event) (model, tea.Cmd) {
	m, cmd := m.applyEventNoWait(ev)
	if m.phase == phaseDone || m.phase == phaseError {
		return m, cmd
	}
	return m, tea.Batch(cmd, waitEvent(m.eventCh))
}

func (m model) applyEventNoWait(ev run.Event) (model, tea.Cmd) {
	switch ev.Kind {
	case run.EventStarted:
		m.started = time.Now()
		if m.phase == phaseBoot {
			m.phase = phaseRunning
		}
		return m, nil

	case run.EventResult:
		if m.phase == phaseBoot {
			m.phase = phaseRunning
		}
		if m.started.IsZero() {
			m.started = time.Now()
		}
		m.finished = ev.Result.Index
		m.results = append(m.results, ev.Result)
		switch ev.Result.Outcome {
		case run.OutcomePass:
			m.passes++
		case run.OutcomeFail:
			m.failures++
			m.fails = append(m.fails, ev.Result)
		case run.OutcomeTimeout:
			m.timeouts++
			m.failures++
			m.fails = append(m.fails, ev.Result)
		}
		m.log = append(m.log, ev.Result)
		if len(m.log) > maxLogLines {
			m.log = m.log[len(m.log)-maxLogLines:]
		}
		return m, nil

	case run.EventDone:
		m.phase = phaseDone
		m.elapsed = time.Since(m.started)
		m.passed = m.failures == 0
		m.writeReport()
		return m, nil

	case run.EventError:
		m.phase = phaseError
		m.err = ev.Err
		if !m.started.IsZero() {
			m.elapsed = time.Since(m.started)
		}
		return m, nil
	}
	return m, nil
}

func (m *model) writeReport() {
	finished := time.Now()
	if m.started.IsZero() {
		m.started = finished
	}
	if err := report.Write(m.cfg.Out, report.Summary{
		Config:   m.cfg,
		Results:  m.results,
		Passes:   m.passes,
		Failures: m.failures,
		Timeouts: m.timeouts,
		Elapsed:  m.elapsed,
		Passed:   m.passed,
		Started:  m.started,
		Finished: finished,
	}); err != nil {
		m.reportErr = err
		m.reportOK = false
		return
	}
	m.reportOK = true
}

func (m model) View() tea.View {
	body := m.mainView()
	if m.phase == phaseSplash {
		body = m.splashView()
	}
	v := tea.NewView(body)
	v.AltScreen = true
	return v
}

func (m model) splashView() string {
	var b strings.Builder
	b.Grow(512)
	b.WriteString(strings.Repeat("\n", clamp(m.height/2-6, 1, 8)))
	b.WriteString(center(styleTitle("▣  RaceGrader"), m.width))
	b.WriteByte('\n')
	b.WriteString(center(styleDim("Catch flaky races before they catch you"), m.width))
	b.WriteString("\n\n")
	b.WriteString(centerBlock(sittingCat(m.tick), m.width))
	b.WriteByte('\n')
	b.WriteString(center(styleMuted(splashStatus(m.tick)), m.width))
	b.WriteByte('\n')
	return b.String()
}

func (m model) mainView() string {
	var b strings.Builder
	b.Grow(2048)
	b.WriteString(styleTitle("▣ RaceGrader"))
	b.WriteString("  ")
	b.WriteString(styleMuted(filepath.Base(m.cfg.Kernel)))
	b.WriteString("  ·  ")
	b.WriteString(styleDim(seedLabel(m.cfg.Seed)))
	b.WriteByte('\n')
	b.WriteString(hrule(m.width))
	b.WriteString("\n\n")

	cat := "\n" + strings.TrimRight(indentBlock(sittingCat(m.tick), 2), "\n")
	b.WriteString(joinHorizontal(cat, m.statusPane(), 3))
	b.WriteString("\n\n")
	b.WriteString(hrule(m.width))
	b.WriteByte('\n')
	b.WriteString(m.renderLog())
	b.WriteByte('\n')
	b.WriteString(hrule(m.width))
	b.WriteByte('\n')
	b.WriteString(styleDim("Ctrl+C Quit"))
	b.WriteByte('\n')
	return b.String()
}

func (m model) statusLine() string {
	switch m.phase {
	case phaseSplash:
		return splashStatus(m.tick)
	case phaseBoot:
		return "Warming up…"
	case phaseRunning:
		return fmt.Sprintf("Running simulation %d / %d", m.finished, m.cfg.Repeat)
	case phaseDone:
		if m.timeouts > 0 && m.finished < m.cfg.Repeat {
			return "Simulation complete (stopped after timeout)"
		}
		return "Simulation complete"
	case phaseError:
		if m.err != nil {
			return m.err.Error()
		}
		return "Stopped"
	default:
		return ""
	}
}

func (m model) statusPane() string {
	bar := progressBar(m.finished, m.cfg.Repeat, clamp(m.width-28, 12, 40))
	status := m.statusLine()
	if m.phase != phaseRunning {
		status = styleText(status)
	}

	var b strings.Builder
	b.Grow(256)
	b.WriteString(status)
	b.WriteString("\n\n")
	b.WriteString(bar)
	b.WriteString("  ")
	b.WriteString(styleDim(fmt.Sprintf("%d/%d", m.finished, m.cfg.Repeat)))
	b.WriteString("\n\n")
	b.WriteString(styleDim(fmt.Sprintf("Pass %d  ·  Fail %d  ·  Timeout %d", m.passes, m.failures, m.timeouts)))
	b.WriteByte('\n')
	b.WriteString(styleDim(fmt.Sprintf("Timeout budget %ds  ·  Repeat %d", m.cfg.Timeout, m.cfg.Repeat)))

	if m.phase == phaseDone {
		b.WriteString("\n\n")
		b.WriteString(styleDim(fmt.Sprintf("Total time %s", formatDur(m.elapsed))))
		b.WriteByte('\n')
		if m.reportOK {
			b.WriteString(styleDim(fmt.Sprintf("Report logged to %s", m.cfg.Out)))
		} else if m.reportErr != nil {
			b.WriteString(styleFail(fmt.Sprintf("Report write failed: %v", m.reportErr)))
		}
	}
	return b.String()
}

func (m model) renderLog() string {
	if m.phase == phaseDone {
		return m.renderEndLog()
	}
	if len(m.log) == 0 {
		return styleDim("  Waiting for first result…")
	}
	var b strings.Builder
	b.Grow(len(m.log) * 64)
	for i, r := range m.log {
		if i > 0 {
			b.WriteByte('\n')
		}
		b.WriteString(formatResultLine(r))
	}
	return b.String()
}

func (m model) renderEndLog() string {
	if len(m.fails) == 0 {
		return styleOk("  No failures") + "\n\n" + styleOk("  PASS")
	}

	maxShow := clamp(m.height-18, 8, len(m.fails))
	start := 0
	var b strings.Builder
	b.Grow(256)
	if len(m.fails) > maxShow {
		start = len(m.fails) - maxShow
		b.WriteString(styleDim(fmt.Sprintf("  … %d earlier failures in report", start)))
		b.WriteByte('\n')
	}
	b.WriteString(styleFail(fmt.Sprintf("  Failures (%d)", len(m.fails))))
	b.WriteByte('\n')
	for _, r := range m.fails[start:] {
		b.WriteString(formatResultLine(r))
		b.WriteByte('\n')
	}
	b.WriteByte('\n')
	b.WriteString(styleFail("  FAIL"))
	return b.String()
}

func formatResultLine(r run.Result) string {
	o := string(r.Outcome)
	return fmt.Sprintf("  %s Simulation #%-4d  %s  seed=%-4d  %6s  %s",
		outcomeMark(o),
		r.Index,
		outcomePaint(o, fmt.Sprintf("%-7s", outcomeLabel(o))),
		r.Seed,
		formatDur(r.Duration),
		styleDim(r.Detail),
	)
}

func center(s string, width int) string {
	w := visibleWidth(s)
	if width <= w {
		return s
	}
	return strings.Repeat(" ", (width-w)/2) + s
}

func centerBlock(block string, width int) string {
	lines := strings.Split(strings.TrimRight(block, "\n"), "\n")
	maxW := 0
	for _, ln := range lines {
		if w := visibleWidth(ln); w > maxW {
			maxW = w
		}
	}
	pad := 0
	if width > maxW {
		pad = (width - maxW) / 2
	}
	prefix := strings.Repeat(" ", pad)
	var b strings.Builder
	b.Grow(len(block) + pad*len(lines) + 1)
	for i, ln := range lines {
		if i > 0 {
			b.WriteByte('\n')
		}
		b.WriteString(prefix)
		b.WriteString(ln)
	}
	b.WriteByte('\n')
	return b.String()
}

func indentBlock(block string, n int) string {
	prefix := strings.Repeat(" ", n)
	lines := strings.Split(strings.TrimRight(block, "\n"), "\n")
	var b strings.Builder
	b.Grow(len(block) + n*len(lines) + 1)
	for i, ln := range lines {
		if i > 0 {
			b.WriteByte('\n')
		}
		b.WriteString(prefix)
		b.WriteString(ln)
	}
	b.WriteByte('\n')
	return b.String()
}

func joinHorizontal(left, right string, gap int) string {
	leftLines := strings.Split(strings.TrimRight(left, "\n"), "\n")
	rightLines := strings.Split(strings.TrimRight(right, "\n"), "\n")
	leftWidth := 0
	for _, ln := range leftLines {
		if w := visibleWidth(ln); w > leftWidth {
			leftWidth = w
		}
	}
	if gap < 1 {
		gap = 1
	}
	n := len(leftLines)
	if len(rightLines) > n {
		n = len(rightLines)
	}
	var b strings.Builder
	b.Grow((leftWidth + gap + 64) * n)
	for i := 0; i < n; i++ {
		l, r := "", ""
		if i < len(leftLines) {
			l = leftLines[i]
		}
		if i < len(rightLines) {
			r = rightLines[i]
		}
		pad := leftWidth - visibleWidth(l) + gap
		if pad < gap {
			pad = gap
		}
		b.WriteString(l)
		b.WriteString(strings.Repeat(" ", pad))
		b.WriteString(r)
		if i < n-1 {
			b.WriteByte('\n')
		}
	}
	return b.String()
}

func clamp(v, lo, hi int) int {
	if v < lo {
		return lo
	}
	if v > hi {
		return hi
	}
	return v
}
