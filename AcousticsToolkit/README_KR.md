# AcousticsToolkit — Project Acoustics(Triton) 바이너리 분석 + C++ 래퍼 + 테스트 하네스

이 폴더는 `ProjectAcousticsArchive`에 들어있는 Microsoft Project Acoustics(내부명 **Triton**)의
**미리 빌드된 바이너리**를 REA(Reverse-Engineer-Anything) MCP로 분석한 결과와,
각 바이너리를 C++에서 안전하게 쓰기 위한 **래퍼 코드 / 단위·통합·종단간 테스트**를 담고 있습니다.

> **읽는 법** — 아래 내용은 세 등급으로 구분해서 적었습니다.
> **[관찰]** 도구가 바이너리에서 직접 읽어낸 사실, **[문서]** Microsoft가 공개한 헤더/Usage의 설명, **[추정]** 위 둘로부터의 해석.

---

## 1. 전체 그림: 베이크(오프라인) → 런타임

```
[에디터/툴]                                    [오프라인 베이크]                            [게임 런타임]

 FBX/메시 ──► Triton.Preprocessor.dll ──►  *.vox + *_config.xml ──► Triton.LocalProcessor.exe ──► *.ace ──► Triton.Runtime.lib
 (재질JSON)   (복셀화, 프로브 배치)             (복셀맵, 프로브/시뮬 설정)   (로컬 PC 에서 파동 시뮬레이션          (프로브 데이터)  QueryAcoustics(src, listener)
                                                          │              + 결과 수집 Collate)                              │
                                                          └──► Triton.BakeService.dll (Azure Batch 로 같은 작업을 클라우드 실행)   ▼
                                                                                                          TritonAcousticParameters
                                                                                                          (직접음/잔향 파라미터)
                                                                                                                │ AcousticsBridge
                                                                                                                ▼
                                                                                                    HrtfDsp.dll (바이노럴/패닝 렌더링)
```

* `Triton.Runtime.lib`는 내부적으로 `Triton.Codec.lib`(→ `zfp.lib`)로 ACE 안의 압축 데이터를 풀어 씁니다.
* `Triton.LocalProcessor.exe`는 `libfbxsdk.dll`, `ClientTelemetry.dll`을 **같은 폴더에서** 로드합니다.

---

## 2. 바이너리별 요약

### 2-1. `Processors/Windows/` (로컬 오프라인 베이크)

| 파일 | 크기 | 종류 | 한 줄 요약 |
|---|---|---|---|
| `Triton.LocalProcessor.exe` | 9.0MB | x64 네이티브 콘솔 EXE | **로컬 베이크 실행기.** config+vox → 시뮬레이션 → *.enc → Collate → *.ace |
| `libfbxsdk.dll` | 10.2MB | Autodesk FBX SDK | FBX 지오메트리 읽기. export **11,347개** (Fbx* C++ 심볼) |
| `ClientTelemetry.dll` | 1.8MB | Microsoft 텔레메트리 SDK | export **263개**, 네임스페이스 `Microsoft::Applications::Events` (EventProperties, LogManager, DebugEventSource…) |
| `RunLocalBake.bat` | — | 배치 | vox/config 를 타임스탬프 폴더에 복사 → `LocalProcessor --configfile` 실행 → `*.enc` 삭제 |
| `Usage.txt` | — | 문서 | CLI 옵션 설명 |

**`Triton.LocalProcessor.exe`** (코드: [`LocalBakeRunner`](include/pa/LocalBakeRunner.h))
* **[문서]** 옵션: `--ConfigFile`(필수) `--WorkingDir` `--SharedInputDataDir` `--OutputName` `--NodeSize`(기본 Local) `--NumThreads` `--Resume`.
* **[관찰]** 문자열에서: `Starting RunTritonTask. Task number: %d of %d, Num threads: %d`, `Starting final task, CollateTritonTask`,
  `Triton.InteropMATLAB.dll`, `SpectralPrecompute: NumPartitions=…`, `Probe placement complete.`, `Num Probes: %d`, `Sim Voxel Size: %.3fm`,
  config 키: `SimFrequency, ReceiverSpacing, DisablePml, ProbeGridSpacingHorizontalMin/Max, ProbeMinHeightAboveGround, NumProbes …`.
* **[관찰]** Import: `VCOMP140.dll`(OpenMP), `libfbxsdk.dll`, `ClientTelemetry.dll`, `MFPlat/MFReadWrite`, `d3d11/D3DCOMPILER_47/USER32`(시각화 계열 심볼).
* **[추정]** Linux 쪽 `Triton.RunTritonTask` + `Triton.CollateTritonData`(둘 다 저장소에 존재)를 한 프로세스로 합친 형태.
  (Azure 쪽 BakeService 가 같은 두 실행 파일을 호출하는 문자열이 있음 — 2-3절)
* **[관찰]** export 는 `cereal` 직렬화 심볼 1개뿐 → 코드를 외부에서 호출하는 용도가 아닌 **프로세스 단위로 실행하는 도구**.

### 2-2. `Source/ThirdParty/Win64/Release/` (UE 플러그인용)

| 파일 | 크기 | 종류 | 한 줄 요약 |
|---|---|---|---|
| `HrtfDsp.dll` (+`.lib`,`.pdb`) | 11.1MB | x64 C-API DLL | **런타임 오디오 DSP**: 다수 모노 소스 → 바이노럴/패닝/공간 리버브 믹스 |
| `Triton.Preprocessor.dll` (+`.lib`,`.pdb`) | 3.6MB | x64 C-API DLL | **베이크 준비**: 음향 메시/재질/복셀화/프로브 배치 → vox+config 생성 |
| `Triton.Runtime.lib` | 3.6MB | 정적 라이브러리 | **런타임 음향 엔진**: ACE 로드/스트리밍/보간/쿼리 (`TritonRuntime::TritonAcoustics`) |
| `Triton.Codec.lib` | 1.3MB | 정적 라이브러리 | ACE 프로브 데이터 디코더 (`Triton::AzfpCodec`) |
| `zfp.lib` | 1.5MB | 정적 라이브러리 | 오픈소스 zfp 부동소수점 압축 (디코드 심볼 197개) |
| `Triton.BakeService.dll` | 56KB | **.NET Framework 4.7.1** 어셈블리 | **Azure Batch 클라우드 베이크 클라이언트** (v2022.1.426.0) |
| `Microsoft.Azure.Batch` 등 9개 DLL | — | .NET | BakeService 의 Azure/JSON 의존성 (이름 기준 **[추정]**) |

#### `HrtfDsp.dll` — 코드: [`HrtfEngine`](include/pa/HrtfEngine.h), [`AcousticsBridge`](include/pa/AcousticsBridge.h)
* **[관찰]** export 12개 전부 `HrtfEngine*`. import 는 KERNEL32/MSVCP140/CRT math 뿐 (d3d·fbx 없음) → 순수 DSP.
* **[문서]** 모노 48kHz float 소스 N개 → 인터리브 float 출력. 엔진 타입: Binaural / Panner / ReverbOnly / PannerOnly / FLEX 계열 / SpatialReverb.
  소스별 `HrtfAcousticParameters`(직접음 방향·게인, 잔향 음량·방향·퍼짐·T60)로 렌더링.
* **[관찰 — 헤더와 DLL 불일치]** 헤더 `HrtfApi.h`는 함수 13개를 선언하지만 이 DLL은 `HrtfEngineGetHasReverbTailRemaining`을 **export하지 않습니다**
  (PE export 표와 `.lib` 심볼 모두 12개). 래퍼는 이 함수를 *선택 기능*으로 처리합니다.
* **[관찰 — 실행 측정]** Panner 엔진에서 `SetOutputFormat(5.1)`은 성공하고 `Process`는 `프레임×6`샘플을 생성하지만,
  `HrtfEngineGetNumOutputChannels`는 계속 **2**를 반환합니다. 래퍼는 `Process(..., outputChannels)` + `ChannelCountForFormat()`로 우회합니다.

#### `Triton.Preprocessor.dll` — 코드: [`PreprocessorClient`](include/pa/PreprocessorClient.h), [`ProceduralScene`](include/pa/ProceduralScene.h)
* **[관찰]** export 27개 = C API 26개 + cereal 심볼 1개. `.lib`도 동일 목록. import: `libfbxsdk.dll`(58개 심볼: FbxMesh/FbxNode/FbxImporter/Triangulate…),
  `VCOMP140`(OpenMP), `d3d11`, `D3DCOMPILER_47`, `USER32/GDI32/IMM32/dwmapi`(UI 계열 심볼).
* **[문서]** 기능 그룹: `AcousticMesh_*`(7) · `MaterialLibrary_*`(8) · `SimulationConfiguration_*`(10) · `SimulationParameters_LoadFromFile`(1).
* **[문서]** 메시 종류: Geometry / Navigation / IncludeVolume / ExcludeVolume / GeometryFillVolume / ProbeSpacingVolume.
* **[관찰 — 실행]** 재질 라이브러리 생성 시 `Default wall absorptivity is set to: 0.100000` 로그를 출력.
* **[관찰]** 같은 폴더에 `Triton.Preprocessor.pdb`(23MB), `HrtfDsp.pdb`(8.8MB)가 있어, 추후 Ghidra/WinDbg 분석 시 함수 이름을 복원할 수 있음.

#### `Triton.Runtime.lib` + `Triton.Codec.lib` + `zfp.lib` — 코드: [`RuntimeAcoustics`](include/pa/RuntimeAcoustics.h)
* **[관찰]** Runtime.lib 심볼 1008개. 주요 클래스(심볼 이름에서): `TritonAcoustics/Impl`, `LoadManager`, `LoaderWorker`, `EncodedMapData`, `EncodedDataCache`,
  `DecodedProbeData`, `PerProbeData`, `ProbeInterpData`, `SpatialProbeDistribution`, `VoxelmapSection`, `ParameterFieldView`,
  `TritonDefault{IO,Mem,AsyncTask}Hook`, `TritonAcousticsDebug`, `QueryDebugInfo`.
* **[관찰]** Codec.lib: `AzfpCodec`, `azfp_block_float_3_fill_mode`, `LU_solve<…>` 템플릿 → zfp 블록 압축 + 소형 LU 선형 풀이.
* **[문서]** 호출 순서 `Init → CreateInstance → InitLoad(ACE) → LoadAll/LoadRegion → QueryAcoustics → Clear → DestroyInstance → TearDown`.
  좌표계는 *Maya Z-up, 미터*. 필요한 프로브가 RAM에 없으면 쿼리 실패.
* **[추정]** 링크 의존 순서 Runtime → Codec → zfp (그리고 이 저장소에서 실제로 **세 개를 함께 링크해야 빌드됨**을 확인).

#### `Triton.BakeService.dll` — (C++ 래퍼 없음: .NET 어셈블리라 C++ 에서 직접 호출하지 않음)
* **[관찰]** .NET Framework 4.7.1, 타입 45개/메서드 149개. 네임스페이스 `Microsoft.Cloud.Acoustics`:
  `CloudProcessor`(핵심), `AzureHelpers`, `AzureAccount`, `ComputePoolConfiguration`, `ComputeCostPredictor`, `ComputeNodeCostInformation`,
  `JobConfiguration`, `JobInformation`, `JobStatus`, `SimulationConfiguration`, `ThreadPoolOperationMonitor<,>`.
* **[관찰]** `CloudProcessor` 공개 메서드(일부): `SubmitForAnalysis(Async)`, `DownloadAceFile(Async)`, `DownloadLogFiles(Async)`, `CheckForAceFileAsync`,
  `GetJobInformation(Async)`, `ListJobs(Async)`, `DeleteJob(Async)`, `DeleteStorage(ForJob)(Async)`, `EstimateProcessingTime`, `InitializeTritonTask`, `InitializeCollatingTask`.
* **[관찰]** 문자열: 컨테이너 이미지 `mcr.microsoft.com/acoustics/baketools:2022.1.Linux`, 노드 `batch.node.ubuntu 20.04`,
  실행 파일 `/acoustics/tools/Triton.RunTritonTask`, `/acoustics/tools/Triton.CollateTritonData`,
  인자 `--ConfigFile {0} --TaskNumber {1} --NodeSize {2} --SharedInputDataDir {3}`, 오토스케일 수식(전용 노드 우선 + 저우선순위 노드), `core.usgovcloudapi.net`(US Gov 클라우드).
* **[추정]** UE 에디터의 Python 스크립트(`init_project_acoustics.py`)가 .NET 으로 이 DLL 을 호출 (README 의 `loadFromRemoteSources` 이슈와 일치).

---

## 3. 코드 파일 지도

```
AcousticsToolkit/
├─ run_tests.bat                  빌드 + 실행 도우미 (인자 없음 = 대화형 테스트 메뉴)
├─ CMakeLists.txt                 빌드 정의 (라이브러리 2개 + 테스트 프로그램 47개 + 메뉴, VS 솔루션 폴더/헤더 표시)
├─ include/pa/  +  src/           ★ 래퍼 라이브러리 (실제 제품 코드)
│   DynamicLibrary                LoadLibrary RAII (DLL 없을 때 SKIP 가능하게)
│   HrtfEngine                    HrtfDsp.dll 래퍼 (엔진 RAII, 소스 관리, Process)
│   PreprocessorClient            Triton.Preprocessor.dll 래퍼 (핸들 RAII, DLL keep-alive, VCOMP140 pin)
│   ProceduralScene               FBX 없이 테스트용 방/바닥 메시를 코드로 생성
│   LocalBakeRunner               LocalProcessor.exe 명령줄 조립/실행/출력 캡처/타임아웃/ACE 탐색
│   RuntimeAcoustics              Triton.Runtime.lib 래퍼 (Init~TearDown RAII, 쿼리, 로그 훅)
│   CoordinateConversion          Unreal ↔ Triton ↔ HRTF 좌표/단위 변환 (플러그인 MathUtils.h 와 동일 공식)
│   AcousticsBridge               TritonAcousticParameters → HrtfAcousticParameters 변환 (참조 구현)
└─ tests/
   ├─ common/                     테스트 프로그램들이 공통으로 쓰는 작은 헤더
   │   Check.h                    PA_BEGIN / PA_STEP / PA_LOG / PA_CHECK / PA_SKIP / PA_PASS 매크로
   │   Helpers.h                  재질 생성, 사인파, 채널 에너지 같은 잔손질 함수 (판정 로직 없음)
   │   CrashTrace.h               네이티브 크래시를 모듈+RVA 로 출력 (모든 테스트에 자동 적용)
   │   TestPaths.h.in             DLL/EXE 경로 (CMake 가 TestPaths.h 로 생성)
   ├─ cases/                      ★ 테스트 1개 = .cpp 1개 = 실행 파일 1개 (각자 main() 이 있다)
   │   unit/          01 ~ 19     바이너리 불필요: ABI, 좌표 변환, 브릿지, 절차적 메시, 명령줄
   │   hrtf/          01 ~ 09     실제 HrtfDsp.dll
   │   preprocessor/  01 ~ 10     실제 Triton.Preprocessor.dll
   │   runtime/       01 ~ 08     Triton.Runtime.lib (정적 링크)
   │   e2e/           01          메시 → 베이크 → ACE → 쿼리 → HRTF 전체 파이프라인
   └─ launcher/                   pa_menu.exe: 테스트 프로그램을 찾아 목록/선택/실행 (테스트 로직 없음)
       main.cpp                   인자 해석
       Launcher.h/.cpp            탐색, --describe 로 설명 수집, 프로세스 실행, 메뉴 루프
       Suites.cpp                 suite 별 제목/설명/필요 파일
```

### 테스트 한 개의 모양 (모든 파일이 같은 형태)

```cpp
int main(int argc, char** argv)
{
    PA_BEGIN("suite.NN_이름", "제목", "무엇을 검증하는가");   // 헤더 출력. "--describe" 이면 설명만 출력하고 종료

    if (!std::filesystem::exists(DLL경로)) PA_SKIP("DLL 이 없음");   // 전제 조건이 없으면 SKIP (종료 코드 77)

    PA_STEP("엔진 생성");              // 큰 단계 로그
    auto engine = ...;                 // 실제 호출
    PA_CHECK(engine != nullptr);       // 틀리면 FAIL 을 출력하고 main 에서 곧바로 return 1

    PA_LOG("에너지 = " << value);      // 세부 값 로그
    return PA_PASS();                  // 끝까지 오면 PASS (종료 코드 0)
}
```

* **이전 구조와의 차이**: 예전에는 테스트를 "등록" 하고 러너가 함수 포인터로 호출했지만, 지금은 테스트가 독립 프로그램이라
  디버거에서 그 파일의 `main()` 첫 줄부터 한 줄씩 따라가면 됩니다. `PA_CHECK` 도 예외가 아니라 `main()` 안에서 곧바로 `return` 합니다.
* 파일 이름 앞의 `NN_` 이 표시/실행 순서입니다. 실행 파일 이름은 `<suite>.<파일이름>.exe` (예: `hrtf.02_source_on_right_is_louder_in_right_ear.exe`).
* 설명의 원본은 각 파일의 `PA_BEGIN(...)` 입니다. 메뉴는 각 실행 파일에 `--describe` 를 주어 제목/설명을 받아 오므로 별도의 목록을 관리하지 않습니다.

### 설계 결정
* **DLL 은 정적 링크 대신 동적 로드**: `.lib` import 방식은 DLL 이 없으면 프로그램이 시작조차 못 합니다. 동적 로드는 *누락된 DLL/export 를 사람이 읽을 수 있는 오류*로 알려주고(테스트는 SKIP), 이번처럼 **헤더와 DLL 이 어긋난 경우(`GetHasReverbTailRemaining`)에도 대응**할 수 있습니다.
* **Runtime.lib 만 정적 링크**: C++ 클래스(`TritonAcoustics`)라 MSVC ABI 가 필요하고 `.lib` 로만 제공됩니다.
* **핸들 수명**: `TritonHandle`(shared_ptr)의 deleter 가 DLL 을 붙잡아 *DLL 이 먼저 내려가는 크래시*를 막습니다 (`handle_keeps_dll_alive_after_client_destroyed` 테스트).

---

## 4. 빌드와 테스트

### 가장 쉬운 방법: `run_tests.bat`

```
AcousticsToolkit\run_tests.bat            대화형 메뉴 (더블클릭해도 됨)
AcousticsToolkit\run_tests.bat all        e2e 제외 전부 실행
AcousticsToolkit\run_tests.bat unit       suite 하나 실행 (unit / hrtf / preprocessor / runtime / e2e)
AcousticsToolkit\run_tests.bat list       테스트 목록(설명 포함)만 출력
AcousticsToolkit\run_tests.bat build      빌드만
AcousticsToolkit\run_tests.bat rebuild    build 폴더를 지우고 처음부터 구성 + 빌드
AcousticsToolkit\run_tests.bat --suite hrtf --filter right_ear    그 밖의 인자는 pa_menu.exe 에 그대로 전달
```

bat 는 cmake 를 찾고(없으면 VS 포함 cmake 탐색) → build 폴더가 없으면 구성 → Release 증분 빌드 → 메뉴 실행까지 자동으로 합니다.
환경변수 `PA_NO_BUILD=1`(빌드 건너뛰기), `PA_NO_PAUSE=1`(종료 시 대기 안 함, 스크립트용)을 지원합니다.

### 대화형 메뉴 (`pa_menu.exe`)

메뉴를 열면 **가능한 모든 테스트가 번호 + 제목 + 검증 내용과 함께** 표시되고, suite 별로 필요한 파일(`HrtfDsp.dll` 등)이 있는지 `[OK]/[없음]`으로 보입니다.
번호를 골라 실행하면 해당 테스트 **프로그램이 별도 프로세스로 실행**되어, 헤더(제목/검증 내용) → **단계 로그(`- 단계`, `| 세부 값`)** → 결과(PASS/FAIL/SKIP) 순으로 출력됩니다.
테스트가 크래시하면 "비정상 종료 + 종료 코드(0xC0000005 등)" 로 표시됩니다.

| 입력 | 의미 |
|---|---|
| `3` / `3 5 7` | 해당 번호 테스트 (여러 개 가능) |
| `21-25` | 범위 |
| `hrtf` | suite 전체 (`unit` `hrtf` `preprocessor` `runtime` `e2e`) |
| `all` | e2e 를 제외한 전부 |
| `l` / `h` / `q` | 목록 다시 보기 / 도움말 / 종료 |

### 한 테스트만 디버깅하기 (브레이크포인트)

1. `AcousticsToolkit\build\AcousticsToolkit.sln` 을 Visual Studio 로 엽니다 (Release 구성 권장: 동봉된 `.lib` 가 `/MD`).
2. 솔루션 탐색기의 `cases` 폴더 → suite 폴더 → 보고 싶은 테스트 프로젝트(예: `hrtf.02_source_on_right_is_louder_in_right_ear`)를
   **우클릭 → "시작 프로젝트로 설정"**.
3. 그 프로젝트의 `.cpp` 안 `main()` 첫 줄(`PA_BEGIN`)에 브레이크포인트를 걸고 **F5**. 한 줄씩 **F10**, 함수 안으로는 **F11**.
   (`HrtfEngine::Create` 등에서 F11 을 누르면 `src/` 의 래퍼 코드로 들어갑니다. DLL 내부로는 들어갈 수 없습니다.)
4. 명령 인수/환경변수가 필요하면 프로젝트 속성 → 디버깅. 예) `PA_PREPROCESSOR_PIN=none`, `PA_TEST_DELAY_MS=150`.

솔루션 탐색기에는 소스뿐 아니라 **우리 헤더(`include/pa`, `tests/common`)와 Triton 공개 헤더(`ThirdParty/Include`)도 폴더 구조대로 표시**됩니다.
`pa_menu` 는 기본 시작 프로젝트입니다.

### 수동 빌드/실행

```powershell
# VS2022 x64. Release 필수 (동봉된 .lib 가 /MD 로 빌드됨)
cmake -S AcousticsToolkit -B AcousticsToolkit/build -G "Visual Studio 17 2022" -A x64
cmake --build AcousticsToolkit/build --config Release -j 8      # 모든 실행 파일은 build\bin\Release\ 에 모인다

AcousticsToolkit\build\bin\Release\pa_menu.exe                   # 콘솔이면 메뉴, 아니면 e2e 제외 전부
AcousticsToolkit\build\bin\Release\pa_menu.exe --list            # 테스트 목록(설명 포함)
AcousticsToolkit\build\bin\Release\pa_menu.exe --suite unit      # suite 실행
AcousticsToolkit\build\bin\Release\hrtf.02_source_on_right_is_louder_in_right_ear.exe   # 테스트 하나를 직접 실행
ctest --test-dir AcousticsToolkit/build -C Release --output-on-failure                 # CTest: 테스트마다 1개 항목(47개)
```

**새 테스트 추가**: `tests/cases/<suite>/NN_이름.cpp` 를 만들고(위의 "테스트 한 개의 모양" 참고) `CMakeLists.txt` 의 해당 `PA_CASES_<SUITE>` 목록에 이름을 추가한 뒤 다시 구성하세요.

| suite | 필요한 것 | 무엇을 검증하나 |
|---|---|---|
| `unit` | 없음 | ABI 레이아웃, 좌표 변환, Triton→HRTF 변환, 절차적 메시, 명령줄 조립 |
| `hrtf` | HrtfDsp.dll | 채널/방향, 좌우 ILD, 잘못된 입력 거부, 포맷별 출력, 선택 export |
| `preprocessor` | Triton.Preprocessor.dll, libfbxsdk.dll | 재질 라이브러리 왕복, 메시 추가, 오류 경로, 핸들 수명 |
| `runtime` | (정적 링크) | Init/TearDown 반복, 데이터 없는 쿼리 실패, 리버브 가중치 |
| `e2e` | 위 전부 + LocalProcessor.exe | 방 메시 → vox/config → 로컬 베이크 → ACE → 쿼리 → HRTF 렌더 |

### 실행 결과 (이 PC: Windows 10, VS2022 MSVC 19.44, Release)

* 테스트 프로그램 **47개 전부 PASS** (unit 19 / hrtf 9 / preprocessor 10 / runtime 8 / e2e 1, CTest 47/47).
* e2e 실측 (8×6×3 m 방, 150 Hz, 프로브 1개): 전처리 13 ms → 로컬 베이크 약 5.6 s (종료 코드 0, `.ace` 생성) →
  런타임 로드(`probes loaded=1 failed=0`) → 쿼리 `path=5.55 m (직선 5.15 m), dry +9.67 dB, wet +8.67 dB, T60 3.18 s, spread 251°` → HRTF 출력 비무음.
  (설정이 극단적으로 작은 테스트용이라 이 수치는 **물리적 정확도의 근거가 아니라 "파이프라인이 끝까지 동작함"의 근거**입니다.)

### 테스트로 드러난 사실 (각각 회귀 테스트로 고정됨)

| # | 발견 | 근거 | 래퍼의 대응 |
|---|---|---|---|
| 1 | `HrtfDsp.dll`은 헤더가 선언한 `HrtfEngineGetHasReverbTailRemaining`을 export하지 않음 (헤더 13개 vs DLL 12개) | PE export 표, `.lib` 심볼, 실제 로드 시 해당 export 누락 | 선택 기능으로 처리 (`SupportsReverbTailQuery()`) |
| 2 | Panner 엔진: `SetOutputFormat` 이후에도 `GetNumOutputChannels`는 2 고정이나 `Process`는 `프레임×포맷채널`을 실제 생성(반환값=전체 샘플 수) | 포맷 6종 × 엔진 2종 실측 | `Process(..., outputChannels)` + `ChannelCountForFormat()` |
| 3 | `Triton.Preprocessor.dll`을 **OpenMP 병렬 구간 직후(~100 ms 안)에 FreeLibrary 하면 세그폴트**. 원인은 Triton 이 아니라 **VCOMP140.DLL 의 워커 스레드** (아래 상세) | 크래시 트레이스 + Ghidra + 통제 실험 | `PreprocessorClient::Load` 가 **VCOMP140.DLL 을 pin** |
| 4 | 프로브 높이 범위는 `[MinHeightAboveGround, +VerticalSpacing]` 이고 **방 밖(천장 위)에도 배치될 수 있음** → 방 안 쿼리가 전부 실패 | 첫 e2e 에서 프로브가 z=4.68 m(방 높이 3 m)에 배치됨 | 테스트 씬의 높이 범위를 방 안으로 제한 |
| 5 | `IncludeVolume` 은 nav 지오메트리와 겹쳐야 하고 최소 프로브 높이를 담을 만큼 높아야 함 | DLL 이 직접 출력한 경고 메시지 | 실패 경로 테스트로 고정 |
| 6 | 재질 라이브러리 생성 시 DLL이 `Default wall absorptivity is set to: 0.100000` 출력 | 실행 로그 | 정보용 |

### 3번 크래시 추적 결과 (Ghidra 12.1.4 + JDK 21 설치 후)

**측정으로 확정된 것**
1. 크래시한 스레드는 전부 **워커 스레드**이고 접근 종류는 `exec`(매핑 해제된 코드 실행)입니다. (`tests/common/CrashTrace.h`, 모든 테스트 프로그램에 자동 적용)
2. 폴팅 주소는 `Triton.Preprocessor.dll` 이미지(베이스 `0x7FFD2CBC0000`, 크기 `0x379000`) **밖**이며, 로드 시점 모듈 목록과 대조하면
   모두 **`VCOMP140.DLL`(MSVC OpenMP 런타임) RVA `0x43AF` / `0x43B8` / `0x43BF`** 입니다. 즉 Triton 코드가 아니라 OpenMP 런타임입니다.
3. Ghidra 디컴파일: 그 RVA는 vcomp140 의 한 함수(`FUN_180004374`) 안이며, "상태값이 2가 될 때까지 최대 100 ms 스핀(`SwitchToThread`) 후
   이벤트를 `WaitForSingleObjectEx(INFINITE)` 로 대기" 하는 동기화 루틴입니다. → OpenMP 워커들이 이 대기 루틴에 있던 중 모듈이 사라졌습니다.
4. PDB 가 적용된 Ghidra 분석(`Triton.Preprocessor.pdb` → 함수 이름 복원): OpenMP `_vcomp_fork` 를 호출하는 함수는 정확히 3개입니다 —
   `Triton::SimulationConfig::GenerateInitialProbes`, `GenerateOracleMeasure`, `SerializeToDisk` (`SerializeToDisk_omp_1` 등 outlined 함수 확인).
   Triton 쪽 코드에는 스레드 정리 로직이 없고, 실패 시에는 `CreateSimulationConfig` 가 `std::runtime_error("Failed to calculate probes")` 를 던질 뿐입니다.
5. **언로드 직전 지연 실험** (pin 없음, 각 6회) — vcomp 의 "100 ms 스핀" 과 정확히 일치:

| 언로드 전 지연 | 프로브 0개 실패 경로 | 성공 경로 |
|---|---|---|
| 0 ms | **6/6 크래시** | **6/6 크래시** |
| 50 ms | 1/6 (워커 AV 60건) | — |
| 150 ms | 0/6 | 0/6 |
| 500 ms | 0/6 | 0/6 |

6. **pin 정책 비교** (지연 0 ms, 각 8회):

| 정책 | 실패 경로 | 성공 경로 |
|---|---|---|
| `none` | 8/8 크래시 | 8/8 크래시 |
| `vcomp` (VCOMP140 만 pin, **기본**) | 0/8 | 0/8 |
| `dll` (Triton DLL 전체 pin) | 0/8 | 0/8 |

**결론**: Triton 의 논리 오류가 아니라 **"OpenMP 병렬 구간 직후 ~100 ms 안에 OpenMP 를 쓰는 DLL 을 언로드하면 vcomp140 워커가 해제된 코드로 복귀해 죽는"** 문제입니다.
그 시간 창을 지나면 워커는 이벤트에서 영구 대기(스레드 누수만 발생)하므로 크래시하지 않습니다.

> **정정**: 이 문서의 이전 버전은 "프로브 배치 실패 경로에서만 발생" 이라고 적었으나 **틀렸습니다**. 성공 경로(e2e)가 안 죽은 것은 언로드 전에 베이크가
> 수 초 걸렸기 때문이고, "겹침 없음" 실패는 병렬 구간에 도달하지 못해서였습니다. 지연 실험으로 바로잡았습니다.
>
> **미확인**: 워커가 정확히 어느 호출(스핀 중 `SwitchToThread`/`GetTickCount64` 복귀 vs. 이벤트 대기 복귀)에서 죽는지는 폴팅 RIP(`+0x43AF` 등)가
> 복귀 주소 위치라는 점만 확인했고, 호출별 구분은 하지 않았습니다. 100 ms 스핀 창과의 일치는 강한 정황이지만 vcomp 내부 소스는 확인하지 못했습니다.

**진단 도구**: 모든 테스트 프로그램은 크래시 시 모듈+RVA 호출 스택을 자동 출력합니다(`tests/common/CrashTrace.h`). 환경변수 `PA_PREPROCESSOR_PIN=vcomp|dll|none`(기본 `vcomp`), `PA_PREPROCESSOR_DIAG=1`, `PA_TEST_DELAY_MS=<ms>`(언로드 직전 대기, preprocessor.08~10).

---

## 5. 분석의 한계 (솔직한 정리)

* **네이티브 함수 내부 분석은 크래시 추적에 필요한 범위로 한정했습니다.** 처음에는 REA의 네이티브 provider(Ghidra 등)가 없어 `provider_unavailable` 이었고,
  이후 Ghidra 12.1.4 + JDK 21 을 설치했습니다 (`rea doctor` 에서 ghidra/headless/decompiler/java 모두 정상). 다만 실행 중이던 REA MCP 서버는 환경변수를 다시 읽지
  않아, Ghidra 분석은 **Ghidra headless 를 직접** 실행했습니다 (REA CLI/MCP 로는 `doctor` 점검까지). 분석한 것은 `Triton.Preprocessor.dll`(+PDB)의
  OpenMP 사용 지점과 실패 경로, 그리고 `vcomp140.dll` 의 크래시 지점뿐입니다. `HrtfDsp.dll` 과 `LocalProcessor.exe` 내부는 디컴파일하지 않았습니다.
* 요약의 나머지 근거는 (a) Microsoft 공개 헤더, (b) PE export/import 표, (c) `.lib` 심볼 표, (d) 문자열, (e) .NET 메타데이터입니다.
* REA로 직접 확인한 것: `Triton.BakeService.dll`의 .NET 메타데이터(타입/메서드/문자열), `Triton.LocalProcessor.exe`의 PE 리소스.
  PE export/import 와 `.lib` 심볼 표는 별도의 읽기 전용 파서(`pedump.py`, 저장소에 포함하지 않음)로 추출했습니다.
* `AcousticsBridge.h`의 변환은 헤더 주석에 근거한 **참조 구현**이며 DLL 내부 로직을 복원한 것이 아닙니다.
* `Triton.RunSingleProbe`, Linux 바이너리 4종은 분석 범위에 포함하지 않았습니다.
