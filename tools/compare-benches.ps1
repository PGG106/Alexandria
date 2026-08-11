param(
    [Parameter(Mandatory = $true)]
    [string]$Baseline,

    [Parameter(Mandatory = $true)]
    [string]$Candidate,

    [int]$Iterations = 10,
    [int]$Depth = 14
)

function Get-Median([double[]]$Values) {
    $sorted = $Values | Sort-Object
    $middle = [int]($sorted.Count / 2)
    if ($sorted.Count % 2) {
        return $sorted[$middle]
    }
    return ($sorted[$middle - 1] + $sorted[$middle]) / 2
}

function Measure-Bench([string]$Name, [string]$Engine) {
    $samples = @()
    $nodes = 0

    for ($iteration = 1; $iteration -le $Iterations; $iteration++) {
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        $output = & $Engine bench $Depth 2>&1
        $stopwatch.Stop()

        $result = $output | Select-String '^\s*(\d+) nodes\s+\d+ nps$' | Select-Object -Last 1
        if (-not $result) {
            throw "$Name did not produce a bench result."
        }

        $nodes = [uint64]$result.Matches[0].Groups[1].Value
        $nps = $nodes / $stopwatch.Elapsed.TotalSeconds
        $samples += $nps
        Write-Host ("{0} run {1}/{2}: {3:N0} nps" -f $Name, $iteration, $Iterations, $nps)
    }

    return [PSCustomObject]@{
        Name = $Name
        Nodes = $nodes
        MedianNps = Get-Median $samples
        MeanNps = ($samples | Measure-Object -Average).Average
    }
}

$baselineResult = Measure-Bench "baseline" $Baseline
$candidateResult = Measure-Bench "candidate" $Candidate

if ($baselineResult.Nodes -ne $candidateResult.Nodes) {
    Write-Warning "Bench node counts differ: baseline=$($baselineResult.Nodes), candidate=$($candidateResult.Nodes)"
}

$change = (($candidateResult.MedianNps / $baselineResult.MedianNps) - 1) * 100
Write-Host ""
Write-Host ("Baseline median:  {0:N0} nps" -f $baselineResult.MedianNps)
Write-Host ("Candidate median: {0:N0} nps" -f $candidateResult.MedianNps)
Write-Host ("Median change:    {0:+0.00;-0.00;0.00}%" -f $change)
