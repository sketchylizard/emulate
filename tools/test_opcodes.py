#!/usr/bin/env python3
"""
6502 Opcode Regression Test Script
Tests all implemented opcodes against the Harte test suite
"""

import argparse
import json
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional, Tuple
from urllib.request import urlretrieve

try:
    from rich.console import Console
    from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn, TimeElapsedColumn
    from rich.table import Table
    from rich.panel import Panel
    RICH_AVAILABLE = True
except ImportError:
    RICH_AVAILABLE = False

# List of implemented opcodes
IMPLEMENTED_OPCODES = [
"00", "08", "09", "0a", "0d", "10", "18", "19", "1d", "28", "29", "2a", "30", "38", "40", "48",
"49", "4a", "4d", "50", "58", "59", "5d", "60", "68", "6a", "70", "78", "88", "8a", "8c", "8d",
"8e", "90", "98", "99", "9a", "9d", "a0", "a2", "a8", "a9", "aa", "ac", "ad", "ae", "b0", "b8",
"b9", "ba", "bc", "bd", "be", "c0", "c5", "c8", "c9", "ca", "cc", "cd", "d0", "d5", "d8", "d9",
"dd", "e0", "e8", "ea", "ec", "f0", "f8", "e4", "c4", "45", "55", "a5", "b5", "a6", "b6", "a4",
"b4", "85", "95", "86", "96", "84", "94", "15", "e6", "f6", "ee", "c6", "d6", "ce", "24", "2c",
"06", "16", "0e", "46", "56", "4e", "26", "36", "2e", "66", "76", "6e", "25", "35", "2d", "20",
"4c", "6c", "39", "3d", "1e", "3e", "5e", "7e", "fe", "de", "05", "21", "c1", "41",
"a1", "81", "01", "11", "31", "51", "91", "b1", "d1",
# ADC
"69", "65", "75", "6d", "7d", "79", "61", "71",
# SBC
"e9", "e5", "f5", "ed", "fd", "f9", "e1", "f1"
]

@dataclass
class TestResult:
    opcode: str
    passed: bool
    skipped: bool = False
    timeout: bool = False
    exit_code: int = 0
    output: str = ""
    duration: float = 0.0
    error_message: str = ""

class OpcodeTestRunner:
    def __init__(self, binary_path: Path, test_data_dir: Path, timeout: int = 30, verbose: bool = False):
        self.binary_path = binary_path
        self.test_data_dir = test_data_dir
        self.timeout = timeout
        self.verbose = verbose
        self.console = Console() if RICH_AVAILABLE else None
        
    def log_info(self, message: str):
        if self.console:
            self.console.print(f"[blue][INFO][/blue] {message}")
        else:
            print(f"[INFO] {message}")
    
    def log_success(self, message: str):
        if self.console:
            self.console.print(f"[green][PASS][/green] {message}")
        else:
            print(f"[PASS] {message}")
    
    def log_warning(self, message: str):
        if self.console:
            self.console.print(f"[yellow][WARN][/yellow] {message}")
        else:
            print(f"[WARN] {message}")
    
    def log_error(self, message: str):
        if self.console:
            self.console.print(f"[red][FAIL][/red] {message}")
        else:
            print(f"[FAIL] {message}")

    def check_prerequisites(self) -> bool:
        """Check if binary and test data directory exist."""
        if not self.binary_path.exists():
            self.log_error(f"Harte test binary not found: {self.binary_path}")
            self.log_info("Try building with: make harte")
            return False
        
        if not self.binary_path.is_file() or not os.access(self.binary_path, os.X_OK):
            self.log_error(f"Harte test binary not executable: {self.binary_path}")
            return False
        
        if not self.test_data_dir.exists():
            self.log_error(f"Test data directory not found: {self.test_data_dir}")
            self.log_info("Run with --download to download test data")
            return False
        
        self.log_info(f"Using binary: {self.binary_path}")
        self.log_info(f"Using test data: {self.test_data_dir}")
        return True

    def download_test_data(self, opcodes: Optional[List[str]] = None) -> bool:
        """Download test data for specified opcodes or all implemented opcodes."""
        if opcodes is None:
            opcodes = IMPLEMENTED_OPCODES
        
        self.log_info(f"Downloading test data to {self.test_data_dir}")
        self.test_data_dir.mkdir(parents=True, exist_ok=True)
        
        downloaded = 0
        failed = 0
        
        base_url = "https://github.com/TomHarte/ProcessorTests/raw/main/6502/v1"
        
        if self.console and RICH_AVAILABLE:
            with Progress(
                SpinnerColumn(),
                TextColumn("[progress.description]{task.description}"),
                BarColumn(),
                TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
                TimeElapsedColumn(),
                console=self.console
            ) as progress:
                task = progress.add_task("Downloading...", total=len(opcodes))
                
                for opcode in opcodes:
                    test_file = self.test_data_dir / f"{opcode}.json"
                    if test_file.exists():
                        if self.verbose:
                            self.log_info(f"Test file for opcode {opcode} already exists")
                        progress.advance(task)
                        continue
                    
                    url = f"{base_url}/{opcode}.json"
                    try:
                        urlretrieve(url, test_file)
                        downloaded += 1
                        if self.verbose:
                            self.log_info(f"Downloaded test for opcode {opcode}")
                    except Exception as e:
                        self.log_warning(f"Failed to download test for opcode {opcode}: {e}")
                        failed += 1
                    
                    progress.advance(task)
        else:
            # Fallback without rich
            for i, opcode in enumerate(opcodes):
                print(f"Downloading {i+1}/{len(opcodes)}: {opcode}")
                test_file = self.test_data_dir / f"{opcode}.json"
                if test_file.exists():
                    continue
                
                url = f"{base_url}/{opcode}.json"
                try:
                    urlretrieve(url, test_file)
                    downloaded += 1
                except Exception as e:
                    self.log_warning(f"Failed to download test for opcode {opcode}: {e}")
                    failed += 1
        
        self.log_success(f"Downloaded {downloaded} test files")
        if failed > 0:
            self.log_warning(f"{failed} downloads failed")
        
        return failed == 0

    def test_single_opcode(self, opcode: str) -> TestResult:
        """Test a single opcode."""
        test_file = self.test_data_dir / f"{opcode}.json"
        
        if not test_file.exists():
            return TestResult(
                opcode=opcode,
                passed=False,
                skipped=True,
                error_message=f"Test file not found: {test_file}"
            )
        
        if self.verbose:
            self.log_info(f"Testing opcode {opcode}...")
        
        start_time = time.time()
        
        try:
            result = subprocess.run(
                [str(self.binary_path), str(test_file)],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            
            duration = time.time() - start_time
            
            if result.returncode == 0:
                if self.verbose:
                    self.log_success(f"Opcode {opcode} passed ({duration:.2f}s)")
                return TestResult(
                    opcode=opcode,
                    passed=True,
                    exit_code=result.returncode,
                    output=result.stdout,
                    duration=duration
                )
            else:
                error_msg = f"Exit code: {result.returncode}"
                if result.stderr:
                    error_msg += f", stderr: {result.stderr.strip()}"
                
                return TestResult(
                    opcode=opcode,
                    passed=False,
                    exit_code=result.returncode,
                    output=result.stdout,
                    duration=duration,
                    error_message=error_msg
                )
        
        except subprocess.TimeoutExpired:
            duration = time.time() - start_time
            return TestResult(
                opcode=opcode,
                passed=False,
                timeout=True,
                duration=duration,
                error_message=f"Timeout after {self.timeout}s"
            )
        
        except Exception as e:
            duration = time.time() - start_time
            return TestResult(
                opcode=opcode,
                passed=False,
                duration=duration,
                error_message=f"Unexpected error: {e}"
            )

    def test_opcodes(self, opcodes: List[str], continue_on_failure: bool = True, 
                    parallel: bool = False, max_workers: int = 4) -> List[TestResult]:
        """Test multiple opcodes."""
        results = []
        
        if parallel and len(opcodes) > 1:
            return self._test_opcodes_parallel(opcodes, max_workers)
        
        if self.console and RICH_AVAILABLE and not self.verbose:
            with Progress(
                SpinnerColumn(),
                TextColumn("[progress.description]{task.description}"),
                BarColumn(),
                TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
                TextColumn("({task.completed}/{task.total})"),
                TimeElapsedColumn(),
                console=self.console
            ) as progress:
                task = progress.add_task("Testing opcodes...", total=len(opcodes))
                
                for opcode in opcodes:
                    result = self.test_single_opcode(opcode)
                    results.append(result)
                    
                    if not result.passed and not result.skipped and not continue_on_failure:
                        self.log_error("Stopping on first failure. Use --continue to test all opcodes.")
                        break
                    
                    progress.advance(task)
        else:
            # Fallback without rich or verbose mode
            for i, opcode in enumerate(opcodes):
                if self.verbose:
                    print(f"\n=== Test {i+1}/{len(opcodes)} ===")
                else:
                    print(f"Testing {i+1}/{len(opcodes)}: {opcode}")
                
                result = self.test_single_opcode(opcode)
                results.append(result)
                
                if not result.passed and not result.skipped and not continue_on_failure:
                    self.log_error("Stopping on first failure. Use --continue to test all opcodes.")
                    break
        
        return results

    def _test_opcodes_parallel(self, opcodes: List[str], max_workers: int) -> List[TestResult]:
        """Test opcodes in parallel."""
        results = []
        
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            # Submit all tasks
            future_to_opcode = {
                executor.submit(self.test_single_opcode, opcode): opcode 
                for opcode in opcodes
            }
            
            if self.console and RICH_AVAILABLE:
                with Progress(
                    SpinnerColumn(),
                    TextColumn("[progress.description]{task.description}"),
                    BarColumn(),
                    TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
                    TextColumn("({task.completed}/{task.total})"),
                    TimeElapsedColumn(),
                    console=self.console
                ) as progress:
                    task = progress.add_task("Testing opcodes (parallel)...", total=len(opcodes))
                    
                    for future in as_completed(future_to_opcode):
                        result = future.result()
                        results.append(result)
                        progress.advance(task)
            else:
                for future in as_completed(future_to_opcode):
                    result = future.result()
                    results.append(result)
                    print(f"Completed: {result.opcode}")
        
        # Sort results by opcode to maintain consistent ordering
        results.sort(key=lambda r: r.opcode)
        return results

    def print_summary(self, results: List[TestResult]) -> bool:
        """Print test summary and return True if all tests passed."""
        passed = sum(1 for r in results if r.passed)
        failed = sum(1 for r in results if not r.passed and not r.skipped)
        skipped = sum(1 for r in results if r.skipped)
        total = len(results)
        
        failed_opcodes = [r.opcode for r in results if not r.passed and not r.skipped]
        skipped_opcodes = [r.opcode for r in results if r.skipped]
        timeout_opcodes = [r.opcode for r in results if r.timeout]
        
        if self.console and RICH_AVAILABLE:
            # Create a nice summary table
            table = Table(title="Test Summary")
            table.add_column("Metric", style="cyan")
            table.add_column("Count", style="magenta")
            table.add_column("Percentage", style="green")
            
            table.add_row("Total Tests", str(total), "100%")
            table.add_row("Passed", str(passed), f"{passed/total*100:.1f}%" if total > 0 else "0%")
            table.add_row("Failed", str(failed), f"{failed/total*100:.1f}%" if total > 0 else "0%")
            table.add_row("Skipped", str(skipped), f"{skipped/total*100:.1f}%" if total > 0 else "0%")
            
            self.console.print(table)
            
            if failed_opcodes:
                self.console.print(Panel(
                    f"Failed opcodes: {', '.join(failed_opcodes)}",
                    title="Failures",
                    border_style="red"
                ))
            
            if timeout_opcodes:
                self.console.print(Panel(
                    f"Timeout opcodes: {', '.join(timeout_opcodes)}",
                    title="Timeouts",
                    border_style="yellow"
                ))
            
            if skipped_opcodes:
                self.console.print(Panel(
                    f"Skipped opcodes: {', '.join(skipped_opcodes)}",
                    title="Skipped",
                    border_style="blue"
                ))
        else:
            # Fallback text summary
            print("\n" + "="*50)
            print("           TEST SUMMARY")
            print("="*50)
            print(f"Total tests:    {total}")
            print(f"Passed:         {passed}")
            print(f"Failed:         {failed}")
            print(f"Skipped:        {skipped}")
            print()
            
            if failed_opcodes:
                print("Failed opcodes:")
                for opcode in failed_opcodes:
                    print(f"  {opcode}")
                print()
            
            if skipped_opcodes:
                print("Skipped opcodes (no test file):")
                for opcode in skipped_opcodes:
                    print(f"  {opcode}")
                print()
        
        # Show detailed failure information in verbose mode
        if self.verbose and failed_opcodes:
            print("\nDetailed failure information:")
            for result in results:
                if not result.passed and not result.skipped:
                    print(f"\nOpcode {result.opcode}:")
                    print(f"  Error: {result.error_message}")
                    if result.output:
                        print(f"  Output: {result.output[:200]}...")
        
        if failed == 0:
            self.log_success("All tests passed! 🎉")
            return True
        else:
            self.log_error(f"{failed} test(s) failed")
            return False

def main():
    parser = argparse.ArgumentParser(
        description="Test 6502 opcodes against the Harte test suite",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                          # Test all implemented opcodes
  %(prog)s -v                       # Test with verbose output
  %(prog)s -o a9                    # Test only LDA immediate (opcode A9)
  %(prog)s --continue               # Continue testing after failures
  %(prog)s --download               # Download missing test files
  %(prog)s --parallel               # Run tests in parallel
  %(prog)s --timeout 60             # Set 60 second timeout per test
        """
    )
    
    parser.add_argument("-v", "--verbose", action="store_true",
                       help="Enable verbose output")
    parser.add_argument("-b", "--binary", type=Path, 
                       default=Path("./build/clang/debug/harte/harte"),
                       help="Path to harte test binary")
    parser.add_argument("-d", "--data-dir", type=Path,
                       default=Path.home() / ".cpm_cache/65x02/1a4b/6502/v1",
                       help="Path to test data directory")
    parser.add_argument("-o", "--opcode", 
                       help="Test only specific opcode (hex, e.g., 'a9')")
    parser.add_argument("-l", "--list", action="store_true",
                       help="List all implemented opcodes and exit")
    parser.add_argument("-c", "--continue", action="store_true", dest="continue_on_failure",
                       help="Continue testing even after failures")
    parser.add_argument("--download", action="store_true",
                       help="Download missing test files")
    parser.add_argument("--timeout", type=int, default=30,
                       help="Timeout per test in seconds (default: 30)")
    parser.add_argument("--parallel", action="store_true",
                       help="Run tests in parallel")
    parser.add_argument("--max-workers", type=int, default=4,
                       help="Maximum parallel workers (default: 4)")
    parser.add_argument("--no-color", action="store_true",
                       help="Disable colored output")
    
    args = parser.parse_args()
    
    # Handle --no-color by disabling rich
    if args.no_color:
        global RICH_AVAILABLE
        RICH_AVAILABLE = False
    
    # Handle --list
    if args.list:
        print(f"Implemented opcodes ({len(IMPLEMENTED_OPCODES)} total):")
        print()
        for i, opcode in enumerate(IMPLEMENTED_OPCODES):
            print(f"{opcode}", end=" ")
            if (i + 1) % 10 == 0:
                print()
        if len(IMPLEMENTED_OPCODES) % 10 != 0:
            print()
        return 0
    
    # Create test runner
    runner = OpcodeTestRunner(
        binary_path=args.binary,
        test_data_dir=args.data_dir,
        timeout=args.timeout,
        verbose=args.verbose
    )
    
    # Handle --download
    if args.download:
        success = runner.download_test_data()
        return 0 if success else 1
    
    # Check prerequisites
    if not runner.check_prerequisites():
        return 1
    
    # Determine which opcodes to test
    if args.opcode:
        opcodes = [args.opcode.lower()]
        runner.log_info(f"Testing single opcode: {args.opcode}")
    else:
        opcodes = IMPLEMENTED_OPCODES
        runner.log_info(f"Testing {len(opcodes)} implemented opcodes...")
    
    # Run tests
    results = runner.test_opcodes(
        opcodes=opcodes,
        continue_on_failure=args.continue_on_failure,
        parallel=args.parallel,
        max_workers=args.max_workers
    )
    
    # Print summary
    success = runner.print_summary(results)
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
