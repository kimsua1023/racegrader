package tui

import (
	"fmt"
	"strings"
	"time"

	"github.com/charmbracelet/x/ansi"
)

var (
	colAccent = ansi.TrueColor(0xE8A87C)
	colMuted  = ansi.TrueColor(0x6B7280)
	colOk     = ansi.TrueColor(0x7DCEA0)
	colFail   = ansi.TrueColor(0xE57373)
	colWarn   = ansi.TrueColor(0xF0C674)
	colText   = ansi.TrueColor(0xE5E7EB)
	colDim    = ansi.TrueColor(0x9CA3AF)
	colCat    = ansi.TrueColor(0xF5D0A9)

	styAccent = ansi.NewStyle().Bold().ForegroundColor(colAccent)
	styMuted  = ansi.NewStyle().ForegroundColor(colMuted)
	styOk     = ansi.NewStyle().ForegroundColor(colOk)
	styFail   = ansi.NewStyle().ForegroundColor(colFail)
	styWarn   = ansi.NewStyle().ForegroundColor(colWarn)
	styText   = ansi.NewStyle().ForegroundColor(colText)
	styDim    = ansi.NewStyle().ForegroundColor(colDim)
	styCat    = ansi.NewStyle().ForegroundColor(colCat)

	cellOn  = styAccent.Styled("█")
	cellOff = styMuted.Styled("░")

	catMaxW = visibleWidth("じしˍ,)ノ")
)

func styleTitle(s string) string { return styAccent.Styled(s) }
func styleMuted(s string) string { return styMuted.Styled(s) }
func styleOk(s string) string    { return styOk.Styled(s) }
func styleFail(s string) string  { return styFail.Styled(s) }
func styleWarn(s string) string  { return styWarn.Styled(s) }
func styleText(s string) string  { return styText.Styled(s) }
func styleDim(s string) string   { return styDim.Styled(s) }

func sittingCat(tick int) string {
	tail := "ノ"
	if tick&1 == 1 {
		tail = "/"
	}
	raw := [4]string{
		" ╱|、",
		"(˚ˎ 。7",
		" |、˜〵",
		"じしˍ,)" + tail,
	}

	var b strings.Builder
	b.Grow(128)
	for i, line := range raw {
		if i > 0 {
			b.WriteByte('\n')
		}
		b.WriteString(styCat.Styled(line))
		if pad := catMaxW - visibleWidth(line); pad > 0 {
			b.WriteString(strings.Repeat(" ", pad))
		}
	}
	return b.String()
}

func splashStatus(tick int) string {
	return "Booting RaceGrader" + [...]string{"   ", ".  ", ".. ", "..."}[(tick/3)%4]
}

func progressBar(done, total, width int) string {
	if total < 1 {
		total = 1
	}
	if width < 8 {
		width = 8
	}
	filled := done * width / total
	if filled > width {
		filled = width
	}
	var b strings.Builder
	b.Grow(width * 16)
	for i := 0; i < width; i++ {
		if i < filled {
			b.WriteString(cellOn)
		} else {
			b.WriteString(cellOff)
		}
	}
	return b.String()
}

func seedLabel(seed int64) string {
	if seed == 0 {
		return "Seed unset"
	}
	return fmt.Sprintf("Seed %d", seed)
}

func formatDur(d time.Duration) string {
	if d < time.Second {
		return fmt.Sprintf("%dms", d.Milliseconds())
	}
	return fmt.Sprintf("%.1fs", d.Seconds())
}

func outcomeLabel(o string) string {
	switch o {
	case "pass":
		return "Pass"
	case "fail":
		return "Fail"
	case "timeout":
		return "Timeout"
	default:
		return o
	}
}

func outcomePaint(o, s string) string {
	switch o {
	case "pass":
		return styleOk(s)
	case "fail":
		return styleFail(s)
	case "timeout":
		return styleWarn(s)
	default:
		return styleDim(s)
	}
}

func outcomeMark(o string) string {
	switch o {
	case "pass":
		return styleOk("✓")
	case "fail":
		return styleFail("✗")
	case "timeout":
		return styleWarn("⏱")
	default:
		return styleDim("·")
	}
}

func visibleWidth(s string) int { return ansi.StringWidth(s) }

func hrule(width int) string {
	return styleMuted(strings.Repeat("─", clamp(width-1, 20, 72)))
}
