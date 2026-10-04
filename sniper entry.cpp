//@version=5
indicator("Gold Bot - Clean Sniper + Zones + Reasons", overlay=true, max_labels_count=500, max_boxes_count=500, max_lines_count=500)

// ==========================================
// 1. INPUTS
// ==========================================
rsiLength = input.int(14, title="RSI Length")
rsiBullishThreshold = input.int(52, title="RSI Bullish Threshold")
rsiBearishThreshold = input.int(48, title="RSI Bearish Threshold")

pivotLookback = input.int(15, title="Pivot Lookback")
zoneWidthPips = input.float(15.0, title="Zone Width", step=0.5)
maxZones = input.int(5, title="Max Zones")
showZones = input.bool(true, title="Show Supply/Demand Zones")

useFVG = input.bool(true, title="Use Fair Value Gaps")
showFVGs = input.bool(true, title="Show FVG Boxes")
maxFVGs = input.int(8, title="Max FVGs")
removeMitigated = input.bool(true, title="Remove FVG when Mitigated")

useEngulfing = input.bool(true, title="Use Engulfing")
usePinBar = input.bool(true, title="Use Pin Bar")
useWickRetrace = input.bool(true, title="Use 50% Wick Retrace")
minWickRatio = input.float(2.5, title="Min Wick to Body Ratio", step=0.5)

useLiquiditySweep = input.bool(true, title="Use Liquidity Sweeps")
sweepLookback = input.int(30, title="Sweep Lookback (bars)")
sweepConfirmBars = input.int(0, title="Bars to Confirm Sweep")
showLiquidity = input.bool(true, title="Show Liquidity Lines")

atrLength = input.int(14, title="ATR Length")
atrMultiplier = input.float(1.5, title="ATR Multiplier for SL")
rrTP1 = input.float(1.0, title="R/R for TP1")
rrTP2 = input.float(2.0, title="R/R for TP2")
rrTP3 = input.float(3.0, title="R/R for TP3")
rrTP4 = input.float(4.0, title="R/R for TP4")

sessionFilter = input.bool(true, title="Only Trade London/NY Sessions")
enableReversal = input.bool(true, title="Allow Instant Reversals")

enableReEntry = input.bool(true, title="Allow Re-Entries")
maxReEntries = input.int(2, title="Max Re-Entries per Trade")
minBarsBetweenEntries = input.int(5, title="Min Bars Between Entries")

// ==========================================
// 2. CALCULATIONS
// ==========================================
rsi = ta.rsi(close, rsiLength)
atr = ta.atr(atrLength)

body = math.abs(close - open)
upperWick = high - math.max(close, open)
lowerWick = math.min(close, open) - low

// ==========================================
// 3. CANDLESTICK PATTERNS
// ==========================================
bullishEngulfing = close > open and close[1] < open[1] and close > open[1] and open < close[1]
bearishEngulfing = close < open and close[1] > open[1] and close < open[1] and open > close[1]

bullishPinBar = lowerWick > (body * minWickRatio) and body > 0 and close > open
bearishPinBar = upperWick > (body * minWickRatio) and body > 0 and close < open

prevLowerWick = math.min(close[1], open[1]) - low[1]
prevUpperWick = high[1] - math.max(close[1], open[1])

bullishWickRetrace = prevLowerWick > 0 and close > (low[1] + (prevLowerWick * 0.5)) and close > open
bearishWickRetrace = prevUpperWick > 0 and close < (high[1] - (prevUpperWick * 0.5)) and close < open

buyConfirmation = (useEngulfing and bullishEngulfing) or (usePinBar and bullishPinBar) or (useWickRetrace and bullishWickRetrace)
sellConfirmation = (useEngulfing and bearishEngulfing) or (usePinBar and bearishPinBar) or (useWickRetrace and bearishWickRetrace)

// ==========================================
// 4. LIQUIDITY SWEEP
// ==========================================
highestHigh = ta.highest(high, sweepLookback)
lowestLow = ta.lowest(low, sweepLookback)

bearishSweep = high > highestHigh[1] and close < highestHigh[1] and close < open
bullishSweep = low < lowestLow[1] and close > lowestLow[1] and close > open

bullishSweepConfirmed = bullishSweep or (bullishSweep[1] and sweepConfirmBars >= 1)
bearishSweepConfirmed = bearishSweep or (bearishSweep[1] and sweepConfirmBars >= 1)

var line topLiqLine = na
var line botLiqLine = na
if showLiquidity and barstate.islast
    line.delete(topLiqLine)
    line.delete(botLiqLine)
    topLiqLine := line.new(bar_index - sweepLookback, highestHigh, bar_index + 30, highestHigh, color=color.new(color.red, 30), width=2, style=line.style_dashed)
    botLiqLine := line.new(bar_index - sweepLookback, lowestLow, bar_index + 30, lowestLow, color=color.new(color.green, 30), width=2, style=line.style_dashed)

// ==========================================
// 5. FVG DETECTION
// ==========================================
var float[] bullFVG_top = array.new_float()
var float[] bullFVG_bot = array.new_float()
var int[] bullFVG_bar = array.new_int()

var float[] bearFVG_top = array.new_float()
var float[] bearFVG_bot = array.new_float()
var int[] bearFVG_bar = array.new_int()

if useFVG and low > high[2] and close[1] > open[1]
    array.push(bullFVG_top, low)
    array.push(bullFVG_bot, high[2])
    array.push(bullFVG_bar, bar_index)
    if array.size(bullFVG_top) > maxFVGs
        array.shift(bullFVG_top)
        array.shift(bullFVG_bot)
        array.shift(bullFVG_bar)

if useFVG and high < low[2] and close[1] < open[1]
    array.push(bearFVG_top, low[2])
    array.push(bearFVG_bot, high)
    array.push(bearFVG_bar, bar_index)
    if array.size(bearFVG_top) > maxFVGs
        array.shift(bearFVG_top)
        array.shift(bearFVG_bot)
        array.shift(bearFVG_bar)

// REMOVE MITIGATED FVGs (with size guards)
if removeMitigated
    if array.size(bullFVG_bot) > 0
        i = 0
        while i < array.size(bullFVG_bot)
            bot = array.get(bullFVG_bot, i)
            if close < bot
                array.remove(bullFVG_top, i)
                array.remove(bullFVG_bot, i)
                array.remove(bullFVG_bar, i)
            else
                i := i + 1
    if array.size(bearFVG_top) > 0
        i = 0
        while i < array.size(bearFVG_top)
            top = array.get(bearFVG_top, i)
            if close > top
                array.remove(bearFVG_top, i)
                array.remove(bearFVG_bot, i)
                array.remove(bearFVG_bar, i)
            else
                i := i + 1

if showFVGs and barstate.islast
    if array.size(bullFVG_top) > 0
        for i = 0 to array.size(bullFVG_top) - 1
            top = array.get(bullFVG_top, i)
            bot = array.get(bullFVG_bot, i)
            b = array.get(bullFVG_bar, i)
            box.new(b, top, bar_index, bot, border_color=color.new(color.blue, 50), bgcolor=color.new(color.blue, 88), border_width=1)
            label.new(b + 1, top, "BULL FVG", color=color.new(color.blue, 70), style=label.style_label_down, textcolor=color.white, size=size.tiny)
    if array.size(bearFVG_top) > 0
        for i = 0 to array.size(bearFVG_top) - 1
            top = array.get(bearFVG_top, i)
            bot = array.get(bearFVG_bot, i)
            b = array.get(bearFVG_bar, i)
            box.new(b, top, bar_index, bot, border_color=color.new(color.purple, 50), bgcolor=color.new(color.purple, 88), border_width=1)
            label.new(b + 1, top, "BEAR FVG", color=color.new(color.purple, 70), style=label.style_label_down, textcolor=color.white, size=size.tiny)

isInsideBullFVG() =>
    bool inside = false
    if array.size(bullFVG_top) > 0
        for i = 0 to array.size(bullFVG_top) - 1
            if low <= array.get(bullFVG_top, i) and close >= array.get(bullFVG_bot, i)
                inside := true
                break
    inside

isInsideBearFVG() =>
    bool inside = false
    if array.size(bearFVG_top) > 0
        for i = 0 to array.size(bearFVG_top) - 1
            if high >= array.get(bearFVG_bot, i) and close <= array.get(bearFVG_top, i)
                inside := true
                break
    inside

inBullFVG = isInsideBullFVG()
inBearFVG = isInsideBearFVG()

// ==========================================
// 6. SUPPLY & DEMAND ZONES
// ==========================================
ph = ta.pivothigh(high, pivotLookback, pivotLookback)
pl = ta.pivotlow(low, pivotLookback, pivotLookback)

var float[] supplyTop = array.new_float()
var float[] supplyBot = array.new_float()
var int[] supplyBar = array.new_int()

var float[] demandTop = array.new_float()
var float[] demandBot = array.new_float()
var int[] demandBar = array.new_int()

if not na(ph)
    array.push(supplyTop, high[pivotLookback])
    array.push(supplyBot, high[pivotLookback] - (zoneWidthPips * 0.1)) 
    array.push(supplyBar, bar_index[pivotLookback])
    if array.size(supplyTop) > maxZones
        array.shift(supplyTop)
        array.shift(supplyBot)
        array.shift(supplyBar)

if not na(pl)
    array.push(demandTop, low[pivotLookback] + (zoneWidthPips * 0.1))
    array.push(demandBot, low[pivotLookback])
    array.push(demandBar, bar_index[pivotLookback])
    if array.size(demandTop) > maxZones
        array.shift(demandTop)
        array.shift(demandBot)
        array.shift(demandBar)

if showZones and barstate.islast
    if array.size(supplyTop) > 0
        for i = 0 to array.size(supplyTop) - 1
            top = array.get(supplyTop, i)
            bot = array.get(supplyBot, i)
            b = array.get(supplyBar, i)
            box.new(b, top, bar_index + 30, bot, border_color=color.new(color.red, 40), bgcolor=color.new(color.red, 90), border_width=1, extend=extend.right)
            label.new(bar_index + 30, top, "SUPPLY", color=color.new(color.red, 40), style=label.style_label_left, textcolor=color.white, size=size.small)
    if array.size(demandTop) > 0
        for i = 0 to array.size(demandTop) - 1
            top = array.get(demandTop, i)
            bot = array.get(demandBot, i)
            b = array.get(demandBar, i)
            box.new(b, top, bar_index + 30, bot, border_color=color.new(color.green, 40), bgcolor=color.new(color.green, 90), border_width=1, extend=extend.right)
            label.new(bar_index + 30, bot, "DEMAND", color=color.new(color.green, 40), style=label.style_label_left, textcolor=color.white, size=size.small)

isInsideDemand() =>
    bool inside = false
    if array.size(demandTop) > 0
        for i = 0 to array.size(demandTop) - 1
            if low <= array.get(demandTop, i) and close >= array.get(demandBot, i)
                inside := true
                break
    inside

isInsideSupply() =>
    bool inside = false
    if array.size(supplyTop) > 0
        for i = 0 to array.size(supplyTop) - 1
            if high >= array.get(supplyBot, i) and close <= array.get(supplyTop, i)
                inside := true
                break
    inside

inDemand = isInsideDemand()
inSupply = isInsideSupply()

// ==========================================
// 7. TRADE STATE
// ==========================================
var bool hasActiveTrade = false
var bool isLong = false
var int reEntryCount = 0
var int lastEntryBar = 0
var float lastSL = na
var float lastTP4 = na

var box currentSLBox = na
var box currentTPBox = na
var line currentEntryLine = na

// ==========================================
// 8. SIGNAL LOGIC
// ==========================================
confirmed = barstate.isconfirmed

isRsiBullish = rsi > rsiBullishThreshold
isRsiBearish = rsi < rsiBearishThreshold

hourNY = hour(time, "America/New_York")
inSession = not sessionFilter or (hourNY >= 3 and hourNY <= 17)

bullishEntry = (inDemand or inBullFVG)
bearishEntry = (inSupply or inBearFVG)

zoneBuySignal = isRsiBullish and bullishEntry and buyConfirmation and inSession
zoneSellSignal = isRsiBearish and bearishEntry and sellConfirmation and inSession

sweepBuySignal = useLiquiditySweep and bullishSweepConfirmed and isRsiBullish and inSession
sweepSellSignal = useLiquiditySweep and bearishSweepConfirmed and isRsiBearish and inSession

rawBuySignal = confirmed and (zoneBuySignal or sweepBuySignal)
rawSellSignal = confirmed and (zoneSellSignal or sweepSellSignal)

canReEnterLong = enableReEntry and hasActiveTrade and isLong and reEntryCount < maxReEntries and (bar_index - lastEntryBar) >= minBarsBetweenEntries
canReEnterShort = enableReEntry and hasActiveTrade and not isLong and reEntryCount < maxReEntries and (bar_index - lastEntryBar) >= minBarsBetweenEntries

reEntryBuy = rawBuySignal and canReEnterLong
reEntrySell = rawSellSignal and canReEnterShort

reversalBuy = enableReversal and hasActiveTrade and not isLong and rawBuySignal
reversalSell = enableReversal and hasActiveTrade and isLong and rawSellSignal

freshBuy = rawBuySignal and not hasActiveTrade
freshSell = rawSellSignal and not hasActiveTrade

// ==========================================
// 9. BUILD REASON STRING
// ==========================================
buyReasons = ""
if inDemand
    buyReasons := buyReasons + (buyReasons == "" ? "Demand" : " + Demand")
if inBullFVG
    buyReasons := buyReasons + (buyReasons == "" ? "FVG" : " + FVG")
if bullishSweepConfirmed
    buyReasons := buyReasons + (buyReasons == "" ? "Sweep" : " + Sweep")
if bullishEngulfing
    buyReasons := buyReasons + (buyReasons == "" ? "Engulfing" : " + Engulfing")
else if bullishPinBar
    buyReasons := buyReasons + (buyReasons == "" ? "Pin Bar" : " + Pin Bar")
else if bullishWickRetrace
    buyReasons := buyReasons + (buyReasons == "" ? "Wick Retrace" : " + Wick Retrace")

sellReasons = ""
if inSupply
    sellReasons := sellReasons + (sellReasons == "" ? "Supply" : " + Supply")
if inBearFVG
    sellReasons := sellReasons + (sellReasons == "" ? "FVG" : " + FVG")
if bearishSweepConfirmed
    sellReasons := sellReasons + (sellReasons == "" ? "Sweep" : " + Sweep")
if bearishEngulfing
    sellReasons := sellReasons + (sellReasons == "" ? "Engulfing" : " + Engulfing")
else if bearishPinBar
    sellReasons := sellReasons + (sellReasons == "" ? "Pin Bar" : " + Pin Bar")
else if bearishWickRetrace
    sellReasons := sellReasons + (sellReasons == "" ? "Wick Retrace" : " + Wick Retrace")

// ==========================================
// 10. TRADE MANAGEMENT
// ==========================================
if hasActiveTrade
    if isLong
        if low <= lastSL
            hasActiveTrade := false
            reEntryCount := 0
            box.delete(currentSLBox)
            box.delete(currentTPBox)
            line.delete(currentEntryLine)
            currentSLBox := na
            currentTPBox := na
            currentEntryLine := na
        else if high >= lastTP4
            hasActiveTrade := false
            reEntryCount := 0
            currentSLBox := na
            currentTPBox := na
            currentEntryLine := na
    else
        if high >= lastSL
            hasActiveTrade := false
            reEntryCount := 0
            box.delete(currentSLBox)
            box.delete(currentTPBox)
            line.delete(currentEntryLine)
            currentSLBox := na
            currentTPBox := na
            currentEntryLine := na
        else if low <= lastTP4
            hasActiveTrade := false
            reEntryCount := 0
            currentSLBox := na
            currentTPBox := na
            currentEntryLine := na

// ==========================================
// 11. EXECUTE TRADES
// ==========================================
if freshBuy
    ep = close
    sl = close - (atr * atrMultiplier)
    rk = ep - sl
    t1 = ep + (rk * rrTP1)
    t4 = ep + (rk * rrTP4)
    hasActiveTrade := true
    isLong := true
    reEntryCount := 0
    lastEntryBar := bar_index
    lastSL := sl
    lastTP4 := t4
    
    currentSLBox := box.new(bar_index, math.max(ep, sl), bar_index + 30, math.min(ep, sl), border_color=color.new(color.red, 40), bgcolor=color.new(color.red, 70), border_width=1)
    currentTPBox := box.new(bar_index, math.max(ep, t4), bar_index + 30, math.min(ep, t4), border_color=color.new(color.green, 40), bgcolor=color.new(color.green, 75), border_width=1)
    currentEntryLine := line.new(bar_index, ep, bar_index + 30, ep, color=color.yellow, width=2, style=line.style_solid)
    
    label.new(bar_index, sl, "SNIPER BUY\n" + buyReasons + "\nENTRY: " + str.tostring(ep, "#.###") + "\nSL: " + str.tostring(sl, "#.###") + "\nTP1: " + str.tostring(t1, "#.###"), color=color.green, style=label.style_label_up, textcolor=color.white, size=size.normal)

if freshSell
    ep = close
    sl = close + (atr * atrMultiplier)
    rk = sl - ep
    t1 = ep - (rk * rrTP1)
    t4 = ep - (rk * rrTP4)
    hasActiveTrade := true
    isLong := false
    reEntryCount := 0
    lastEntryBar := bar_index
    lastSL := sl
    lastTP4 := t4
    
    currentSLBox := box.new(bar_index, math.max(ep, sl), bar_index + 30, math.min(ep, sl), border_color=color.new(color.red, 40), bgcolor=color.new(color.red, 70), border_width=1)
    currentTPBox := box.new(bar_index, math.max(ep, t4), bar_index + 30, math.min(ep, t4), border_color=color.new(color.green, 40), bgcolor=color.new(color.green, 75), border_width=1)
    currentEntryLine := line.new(bar_index, ep, bar_index + 30, ep, color=color.yellow, width=2, style=line.style_solid)
    
    label.new(bar_index, sl, "SNIPER SELL\n" + sellReasons + "\nENTRY: " + str.tostring(ep, "#.###") + "\nSL: " + str.tostring(sl, "#.###") + "\nTP1: " + str.tostring(t1, "#.###"), color=color.red, style=label.style_label_down, textcolor=color.white, size=size.normal)

if reEntryBuy
    ep = close
    sl = close - (atr * atrMultiplier)
    rk = ep - sl
    t1 = ep + (rk * rrTP1)
    t4 = ep + (rk * rrTP4)
    reEntryCount := reEntryCount + 1
    lastEntryBar := bar_index
    lastSL := sl
    lastTP4 := t4
    
    label.new(bar_index, sl, "RE-ENTRY BUY #" + str.tostring(reEntryCount) + "\n" + buyReasons + "\nENTRY: " + str.tostring(ep, "#.###") + "\nSL: " + str.tostring(sl, "#.###") + "\nTP1: " + str.tostring(t1, "#.###"), color=color.new(color.green, 20), style=label.style_label_up, textcolor=color.white, size=size.small)

if reEntrySell
    ep = close
    sl = close + (atr * atrMultiplier)
    rk = sl - ep
    t1 = ep - (rk * rrTP1)
    t4 = ep - (rk * rrTP4)
    reEntryCount := reEntryCount + 1
    lastEntryBar := bar_index
    lastSL := sl
    lastTP4 := t4
    
    label.new(bar_index, sl, "RE-ENTRY SELL #" + str.tostring(reEntryCount) + "\n" + sellReasons + "\nENTRY: " + str.tostring(ep, "#.###") + "\nSL: " + str.tostring(sl, "#.###") + "\nTP1: " + str.tostring(t1, "#.###"), color=color.new(color.red, 20), style=label.style_label_down, textcolor=color.white, size=size.small)

if reversalBuy
    ep = close
    sl = close - (atr * atrMultiplier)
    rk = ep - sl
    t1 = ep + (rk * rrTP1)
    t4 = ep + (rk * rrTP4)
    isLong := true
    reEntryCount := 0
    lastEntryBar := bar_index
    lastSL := sl
    lastTP4 := t4
    
    currentSLBox := box.new(bar_index, math.max(ep, sl), bar_index + 30, math.min(ep, sl), border_color=color.new(color.red, 40), bgcolor=color.new(color.red, 70), border_width=1)
    currentTPBox := box.new(bar_index, math.max(ep, t4), bar_index + 30, math.min(ep, t4), border_color=color.new(color.green, 40), bgcolor=color.new(color.green, 75), border_width=1)
    currentEntryLine := line.new(bar_index, ep, bar_index + 30, ep, color=color.yellow, width=2, style=line.style_solid)
    
    label.new(bar_index, sl, "REVERSAL BUY\n" + buyReasons + "\nENTRY: " + str.tostring(ep, "#.###") + "\nSL: " + str.tostring(sl, "#.###") + "\nTP1: " + str.tostring(t1, "#.###"), color=color.new(color.green, 20), style=label.style_label_up, textcolor=color.white, size=size.normal)

if reversalSell
    ep = close
    sl = close + (atr * atrMultiplier)
    rk = sl - ep
    t1 = ep - (rk * rrTP1)
    t4 = ep - (rk * rrTP4)
    isLong := false
    reEntryCount := 0
    lastEntryBar := bar_index
    lastSL := sl
    lastTP4 := t4
    
    currentSLBox := box.new(bar_index, math.max(ep, sl), bar_index + 30, math.min(ep, sl), border_color=color.new(color.red, 40), bgcolor=color.new(color.red, 70), border_width=1)
    currentTPBox := box.new(bar_index, math.max(ep, t4), bar_index + 30, math.min(ep, t4), border_color=color.new(color.green, 40), bgcolor=color.new(color.green, 75), border_width=1)
    currentEntryLine := line.new(bar_index, ep, bar_index + 30, ep, color=color.yellow, width=2, style=line.style_solid)
    
    label.new(bar_index, sl, "REVERSAL SELL\n" + sellReasons + "\nENTRY: " + str.tostring(ep, "#.###") + "\nSL: " + str.tostring(sl, "#.###") + "\nTP1: " + str.tostring(t1, "#.###"), color=color.new(color.red, 20), style=label.style_label_down, textcolor=color.white, size=size.normal)