#!/usr/bin/env bash
set -euo pipefail

kernel_path="${INPUT_KERNEL_PATH:-kernel}"
working_directory="${INPUT_WORKING_DIRECTORY:-.}"
repeat="${INPUT_REPEAT:-100}"
timeout="${INPUT_TIMEOUT:-30}"
seed="${INPUT_SEED:-0}"
cpus="${INPUT_CPUS:-2}"
chaos="${INPUT_CHAOS:-1}"
command="${INPUT_COMMAND:-cow_test}"
out="${INPUT_OUT:-}"
write_summary="${INPUT_WRITE_SUMMARY:-true}"
GITHUB_WORKSPACE="${GITHUB_WORKSPACE:-/github/workspace}"

cd "${GITHUB_WORKSPACE}/${working_directory}"
abs_kernel="$(realpath -m "${kernel_path}")"
workspace="$(realpath -m "${GITHUB_WORKSPACE}")"

case "${abs_kernel}" in
"${workspace}"/*) ;;
*)
  echo "kernel_path must stay inside the repository workspace: ${kernel_path}" >&2
  exit 1
  ;;
esac

if [[ ! -d "${abs_kernel}" ]]; then
  echo "kernel directory not found: ${abs_kernel}" >&2
  exit 1
fi

if [[ -n "${out}" ]]; then
  report_path="$(realpath -m "${out}")"
  mkdir -p "$(dirname "${report_path}")"
else
  report_path="$(mktemp /tmp/racegrader-XXXXXX.md)"
fi

args=(
  run
  --kernel "${abs_kernel}"
  --repeat "${repeat}"
  --timeout "${timeout}"
  --out "${report_path}"
  --cpus "${cpus}"
  --chaos "${chaos}"
  --command "${command}"
  --headless
)
if [[ "${seed}" != "0" ]]; then
  args+=(--seed "${seed}")
fi

echo "racegrader ${args[*]}"
set +e
racegrader "${args[@]}"
cli_exit=$?
set -e

case "${write_summary}" in
false | FALSE | 0 | no | NO) ;;
*)
  if [[ -n "${GITHUB_STEP_SUMMARY:-}" ]]; then
    if [[ -f "${report_path}" ]]; then
      cat "${report_path}" >> "${GITHUB_STEP_SUMMARY}"
    else
      echo "_RaceGrader produced no report._" >> "${GITHUB_STEP_SUMMARY}"
    fi
  fi
  ;;
esac

exit "${cli_exit}"
