# ADAS Computer Vision: The Complete Pipeline Landscape
### Data Engineering → Data Science/Training → Testing → Inference/Deployment (all hardware + cloud) → How Competitors Actually Do It

This is a companion to the earlier ADAS pipeline guide — this one widens the aperture to (1) every mainstream option at each stage, cloud included, and (2) how the named industry players (Tesla, Waymo, Mobileye, NVIDIA-ecosystem OEMs, Wayve, comma.ai, Zoox) actually build theirs, with sources.

---

## PART A — The Full Option Space, Stage by Stage

### A.1 Data Engineering

**Fleet ingestion patterns (pick based on fleet size/connectivity)**
| Pattern | How it works | Who uses it |
|---|---|---|
| Physical hard-drive swap + local pre-processing | Loggers write to SSDs; drives pulled at depot, copied via edge appliance | Common for dedicated LiDAR-heavy test fleets |
| Edge pre-processing appliance + cloud upload | **AWS Outposts** at the garage/depot for local dedup/compression before upload to S3 | AWS's published Autonomous Driving & ADAS Data Lake reference architecture, built with patterns like BMW's Cloud Data Hub |
| Continuous over-the-air telemetry (consumer fleet) | Small flagged clips streamed opportunistically over cellular from millions of production cars | Tesla's shadow-mode/fleet-learning pipeline |
| Direct high-bandwidth cloud ingestion | Vehicle data infrastructure streaming multi-TB/hour direct to object storage for near-real-time training | Zoox's AWS pipeline reportedly ingests **up to 4 TB of vehicle data per hour** via AWS Data Transfer during foundation-model training |

**Storage & processing**
- Object storage as the system of record: **Amazon S3** (with 50 TB max object size and S3 Vectors for embedding-scale search as of late 2025), **Azure Data Lake Storage**, **Google Cloud Storage**
- Large-scale batch processing: **Apache Spark** (via AWS EMR, Azure Databricks, GCP Dataproc) for petabyte-scale scenario extraction/metadata tagging
- Format: ROS2 bag/MCAP at capture time → WebDataset/TFRecord/Parquet for training-time sharded access
- Data catalog / metadata / automated scenario detection layer sits on top so engineers can query "all clips with cut-ins at night in rain" — this is the single most differentiating piece of infrastructure between a company that can scale and one that can't

**Labeling**
- Managed labeling services: **Amazon SageMaker Ground Truth** (with auto-labeling), Scale AI, Appen
- Self-hosted: **CVAT**, **Label Studio**, **SUSTechPOINTS** (3D box)
- Auto-labeling via a heavier, non-real-time "teacher" model — now the industry-standard cost lever (used by Tesla, Waymo, and most Tier-1s in some form) — human review only on a curated subset

### A.2 Applying Data Science (methodology choices)

There are three broad architectural philosophies in production today — this is the single biggest "how do I build this" decision:

| Philosophy | Description | Representative examples |
|---|---|---|
| **Modular pipeline** (perception → prediction → planning as separate, interpretable stages) | Each stage trained/validated independently; easiest to certify against ISO 26262/21448 because you can unit-test each block | Most Tier-1/OEM L2 ADAS stacks (Mobileye, Bosch, Continental); NVIDIA DRIVE's more classic configurations |
| **End-to-end learned driving** (single network: pixels/sensors in, trajectory/controls out) | Removes hand-engineered interfaces between modules; harder to certify/debug because failure modes are less interpretable | Tesla FSD v12+ (single neural network replacing ~300k lines of C++, per Tesla's own public statements); comma.ai openpilot's "Supercombo" model |
| **Foundation / world-model + VLA (vision-language-action)** hybrid | A generative world model learns to predict future sensor states / is used to generate training and eval scenarios; a VLA model reasons over language + vision for planning | Wayve's GAIA-1/GAIA-2 world models + LINGO-2 VLA driving model <cite index="73-1,78-1">GAIA-1 was described as the first generative AI foundation model built for driving, with LINGO-2 following as the first closed-loop vision-language-action driving model tested on public roads</cite>; NVIDIA's Alpamayo (open VLA reasoning model) + Cosmos world foundation models |

**Multi-task learning is close to universal** across all three philosophies — shared backbone, multiple heads (detection, segmentation, depth, occupancy, trajectory) trained jointly to fit real-time compute budgets. Kendall et al.'s uncertainty-weighted multi-task loss (CVPR 2018) remains the standard reference for balancing these heads.

**Simulation-augmented training** is now standard practice across nearly every serious player, not just an afterthought: real logs get reconstructed into interactive 3D scenes (NVIDIA Omniverse NuRec uses 3D Gaussian Splatting) and then re-rendered from novel viewpoints/trajectories to multiply the value of every real "interesting" clip — described by NVIDIA as turning one bad real-world case into "a seed crystal for many tests."

### A.3 Training Infrastructure — Cloud, On-Prem, and Hybrid Options

| Option | Notes |
|---|---|
| **On-prem GPU clusters** | Still common for the largest players who want full control/cost predictability at scale |
| **Custom silicon** | Tesla's Dojo supercomputer, built on custom D1 chips, specifically for training the unified end-to-end model on 4D (width, height, time, feature) video tensors |
| **AWS** | Amazon SageMaker (training, Ground Truth labeling, Autopilot), **SageMaker HyperPod** for large distributed foundation-model training — Zoox reports 95% GPU utilization across 64+ GPUs using Hybrid Sharded Data Parallelism + tensor parallelism on HyperPod; AWS Trainium3 UltraServers for next-gen training/inference; AWS Batch for large-scale simulation/reprocessing jobs |
| **Microsoft Azure** | Azure Machine Learning, Azure Databricks for data processing, Azure AI infrastructure |
| **Google Cloud** | Vertex AI, TPU pods, Dataproc/BigQuery for data engineering |
| **NVIDIA DGX Cloud / on-prem DGX** | Positioned explicitly as one of NVIDIA's "three computers" for AV development — DGX for training, Omniverse/Cosmos for simulation, DRIVE AGX for in-vehicle inference |

Distributed training details: PyTorch DDP/FSDP or framework-native equivalents (SageMaker's data-parallel/model-parallel libraries, reporting up to 93% scaling efficiency across 64 GPUs in Hyundai's published case study); mixed precision as default.

### A.4 Testing & Validation — recap + simulation-cloud options
(See the companion ADAS guide for the full MIL/SIL/HIL breakdown and ISO 26262/21448/8800 framing.) Cloud/simulation-specific additions:
- **NVIDIA Omniverse Cloud / Cosmos** — cloud-hosted high-fidelity sensor simulation and synthetic scenario generation at scale
- **CARLA** — open-source, can run on-prem or in any cloud VM with GPU
- **AWS Batch + Omniverse/CARLA on EC2 GPU instances** — large parallel scenario-replay test farms
- Regression gating: every model candidate re-run against the full scenario catalog + historical failure cases before promotion — standard safety-critical CI practice

### A.5 Inference & Deployment — the Full Hardware/Runtime Matrix

This is the part that varies most by target vehicle program, so here's the broadest possible view:

| Target class | Hardware examples | Native/typical runtime | Notes |
|---|---|---|---|
| **NVIDIA-based domain controllers** | Jetson (dev/robotics), DRIVE AGX Orin, DRIVE AGX Thor | **TensorRT**, DriveOS (bundles DriveWorks, CUDA, TensorRT), ONNX Runtime w/ TensorRT EP | DRIVE Hyperion is NVIDIA's full reference vehicle architecture bundling compute + sensor suite + safety stack; adopted by Mercedes-Benz, BYD, Jaguar Land Rover, Geely, Isuzu, Nissan for L2++ to L4 programs |
| **Qualcomm Snapdragon Ride** | Snapdragon Ride SoCs (automotive Hexagon NPU) | **QNN (Qualcomm AI Engine Direct / QAIRT)** — current recommended stack, replacing legacy SNPE; accessed via ONNX Runtime's QNN Execution Provider or native QNN SDK | INT8 weights + 16-bit activations is the typical Hexagon NPU sweet spot |
| **TI Jacinto (TDA4/new TDA5)** | TDA4VH-Q1, TDA4VPE-Q1 (L2/L2+), new TDA5 family (up to 1,200 TOPS, C7 NPU, ASIL-D-capable) | TI's proprietary edge-AI SDK on top of the C7 NPU; software largely portable from TDA4 to TDA5 | STRADVISION's SVNet is a widely deployed third-party perception stack that ships on top of TDA4/TDA5 |
| **Ambarella CVflow** | CV7 (vision/edge AI), CV3 (AI central domain controller for AD) | Ambarella's proprietary CVflow toolchain; increasingly supports transformer as well as CNN workloads | CV3 adds sensor fusion and planning layers on top of camera perception |
| **Mobileye EyeQ** | EyeQ4/EyeQ5/EyeQ6/EyeQ Ultra | Proprietary Mobileye stack (not third-party open) | Runs Mobileye's own vision networks, REM crowdsourced mapping, and the dual-subsystem "True Redundancy" architecture (camera subsystem + independent radar/LiDAR subsystem) |
| **ARM Cortex/Ethos-based ECUs** | Lower-tier ADAS ECUs | ARM NN, Ethos-U driver stack | Common where cost/power budget is tightest |
| **FPGA / custom ASIC** | Xilinx/AMD automotive FPGAs, custom silicon | Vitis AI (Xilinx/AMD), vendor-specific compilers | Used where deterministic latency or a locked-down bill-of-materials matters more than flexibility |
| **Cross-platform baseline** | Any of the above | **ONNX Runtime** with CPU/CUDA/TensorRT/DirectML/OpenVINO/QNN Execution Providers | Best portability; often used as the common export target before a final vendor-specific compile step; can lag a fully native compiled path (e.g., raw TensorRT) on latency-critical targets |
| **Cloud-side / off-vehicle inference** | Any of the above cloud platforms | Used for shadow-mode evaluation, fleet-scale re-scoring of candidate models against logged data, remote assistance/teleoperation decision support — never the safety-critical real-time control loop itself | This is a genuinely distinct deployment target: same model, but the "deployment" is a cloud batch/streaming job scoring millions of driving hours, not a vehicle ECU |

**Practical export/compile gotchas seen across vendors**: custom or newer ops (deformable attention, `Mod` ops in recent YOLO variants) frequently aren't supported by a given NPU compiler on the first try — the standard fixes are operator patching, custom op registration, or falling back to a vendor-specific DLC/compiler path instead of the generic ONNX route.

### A.6 Cloud-Side MLOps and OTA
- Pipeline orchestration: **SageMaker Pipelines**, **Azure ML Pipelines**, **Vertex AI Pipelines**, or open-source **Kubeflow**/**MLflow**
- Model/data versioning: **DVC**, **MLflow Model Registry** — required evidence for safety-case traceability, not just convenience
- OTA update pipeline: staged rollout (canary fleet → shadow mode validation on production fleet → full rollout), with an automated regression gate against the full scenario/test-case catalog before promotion

---

## PART B — How Named Competitors Actually Build This (with sources)

### B.1 Tesla — fleet-scale end-to-end learning
- **Data engine**: <cite index="45-1">Tesla's data engine auto-labels millions of short video clips in bulk to train vision networks, curates a subset for edge cases, and pushes updated models to cars where they're observed in "shadow mode" before being refined further</cite>. <cite index="45-1">Public descriptions of these pipelines cite datasets on the order of 1.5 petabytes drawn from roughly 1 million clips, backed by large GPU clusters.</cite>
- **Shadow mode**: <cite index="46-1">The fleet runs the FSD stack continuously in the background, comparing its intended actions to the human driver's actual actions; when a driver intervenes in a way the system didn't anticipate — a "hard clip" — that scenario is automatically flagged for collection as one of the most valuable, novel edge cases.</cite>
- **Training compute**: <cite index="46-1">Tesla's Dojo supercomputer, built on custom D1 chips, is specifically designed for the low-latency, high-bandwidth communication needed to train a unified end-to-end model on 4D (width, height, time, feature) video tensors.</cite>
- **Architecture shift**: <cite index="49-1">Tesla's FSD v12 abandoned the traditional rules-based paradigm — Musk stated version 11 had over 300,000 lines of C++ and version 12 has basically none of that, learning instead by observing millions of hours of human driving and outputting steering/acceleration/braking directly from raw camera input through a single neural network pipeline, across 48 distinct neural networks processing input from 8 cameras.</cite>
- **Reference papers/talks**: Tesla AI Day 2021/2022 technical presentations (occupancy networks, planning via neural nets); no peer-reviewed publication trail comparable to Waymo/academic AV labs — most public technical detail comes from AI Day talks and Tesla engineering blog posts rather than conference papers.

### B.2 Waymo — large-scale multimodal perception, published benchmarks
- Waymo is the most academically transparent of the major AV players, publishing its dataset and baseline methods at CVPR.
- <cite index="57-1">The Waymo Open Dataset paper reports around 12 million LiDAR box annotations and around 12 million camera box annotations, giving rise to roughly 113k LiDAR object tracks and about 250k camera image tracks, with all ground truth boxes containing track identifiers to support object tracking.</cite>
- <cite index="61-1">Waymo's own baseline detector reuses a convolutional backbone with a stride-1 first block for both vehicle and pedestrian models, operating at 512×512 spatial resolution, trading extra compute for accuracy, with anchor sizes tuned per class and a smooth-L1 heading-residual loss wrapped to [−π, π].</cite>
- Reference paper: Sun et al., *Scalability in Perception for Autonomous Driving: Waymo Open Dataset*, CVPR 2020.
- Recent extension: WOD-E2E dataset for end-to-end driving in long-tail scenarios (2025), reflecting Waymo's own move toward evaluating BEV-centric and MLLM-based end-to-end planners alongside its classic modular stack.

### B.3 Mobileye — camera-first, redundant-subsystem, crowdsourced mapping
- <cite index="63-1">Mobileye's three pillar technologies are True Redundancy sensing, REM (Road Experience Management) crowdsourced mapping, and Responsibility-Sensitive Safety (RSS).</cite>
- <cite index="67-1">True Redundancy is an integrated system using data streams from 360-surround cameras, LiDAR, and radar, adding a lidar/radar subsystem to the computer-vision subsystem for redundancy; Mobileye's SuperVision system instead uses cameras only, running on the EyeQ5 SoC with 11 cameras for hands-off driving.</cite>
- <cite index="69-1">REM turns HD-mapping into a byproduct of ordinary driving: EyeQ4-equipped consumer vehicles identify and process lane markings, curbs, landmarks, traffic signs, and other infrastructure, translating this into roughly 10 kilobytes of compressed data per kilometer driven, rather than requiring dedicated mapping fleets with expensive sensors.</cite>
- <cite index="65-1">By 2025 Mobileye had REM coverage across nearly the entire navigable road network in the U.S. and Europe, and had secured a patent portfolio exceeding 2,000 filings — real IP barrier evidence you can verify directly via the Justia/Google Patents links below.</cite>
- Patent search: [Mobileye Vision Technologies patents on Justia](https://patents.justia.com/assignee/mobileye-vision-technologies-ltd) — includes real, verifiable filings such as crowd-sourced map generation methods and neural-network-unit memory-traffic reduction techniques.

### B.4 NVIDIA DRIVE ecosystem (used by many OEMs rather than being a single company's stack)
- <cite index="83-1">NVIDIA frames AV development as needing three computers: DGX systems for training advanced AI models and building the AV software stack in the cloud, the Omniverse platform running on OVX systems for simulation and validation, and the DRIVE AGX in-vehicle computer — now joined by the Cosmos world foundation model platform.</cite>
- <cite index="85-1">DRIVE Hyperion is NVIDIA's reference vehicle architecture bundling compute, sensors, and safety assumptions into a template OEMs build on: Hyperion 8 is built on the Orin generation, while Hyperion 10 is built on dual Thor chips with lidar, more cameras/radars, more interior sensing, and redundancy to keep operating if something fails.</cite>
- <cite index="85-1">NVIDIA's simulation loop reconstructs real failure scenes with Omniverse NuRec, generates more variation with Cosmos world models, and runs policies closed-loop in simulation — turning one real bad case into a seed for many synthetic test variants.</cite>
- <cite index="90-1">As of early 2026, DRIVE Hyperion adoption includes BYD, Geely, Isuzu, and Nissan for Level 4 programs, standardized on the platform together with NVIDIA's Halos safety-architecture layer.</cite>
- Alpamayo is NVIDIA's open vision-language-action (VLA) reasoning model family for L4 robotaxi development, explicitly positioned as a teacher/research foundation rather than a complete production stack.

### B.5 Wayve — end-to-end + generative world models
- <cite index="78-1">Wayve's GAIA-1 was presented as the first generative AI foundation model built specifically for driving, aimed at helping autonomous systems understand, predict, and adapt to the complexity of the real world.</cite>
- <cite index="73-1">LINGO-2, released in 2024, is described as the first closed-loop vision-language-action driving model tested on public roads.</cite> GAIA-2 (2025) extended this to a controllable multi-view generative world model for creating diverse, safety-critical driving scenarios at scale.
- Wayve's published research trail (all with arXiv/technical reports): LingoQA (video QA for driving), CarLLaVA (vision-language model for camera-only closed-loop driving), SimLingo (vision-only closed-loop driving with language-action alignment), GAIA-2, and the newer GAIA-3/Rig3R/PRISM-1/LA-Pose lines extending world-model scale and 3D perception.
- Wayve's approach is explicitly camera-only end-to-end, positioned as an alternative both to Tesla's non-published approach and to the modular LiDAR-heavy stacks of Waymo/Mobileye.

### B.6 comma.ai / openpilot — the one fully open-source, inspectable case study
This is the most useful reference if you want to see an actual production ADAS pipeline's implementation details rather than a marketing description.
- <cite index="109-1">In contrast to traditional autonomous driving solutions where perception, prediction, and planning are separate modules, openpilot uses a system-level end-to-end neural network to predict the car's trajectory directly from camera images, trained by comma.ai using real-world driving data uploaded by openpilot users.</cite>
- <cite index="102-1">The shipped model is exported to ONNX; you can inspect its exact architecture yourself with Netron, and the model consumes a two-frame YUV 4:2:0 buffer plus a rolling 5-second/512-dim temporal feature context, and can run through either an ONNX runner (float32) or an SNPE runner (uint8) depending on target hardware.</cite>
- <cite index="107-1">The backbone lineage is documented publicly: early openpilot models used ResNet18, later moving to EfficientNet-B2, with the combined model nicknamed "supercombo" because it also includes a pose-net used for lead-vehicle prediction and vision-only velocity estimation.</cite>
- Independent academic reproduction/analysis exists too: Chen et al., *Level 2 Autonomous Driving on a Single Device: Diving into the Devils of Openpilot*, arXiv:2206.08176 (2022) — a full deep-dive re-implementation and benchmark against nuScenes/Comma2k19/CARLA, with [open-source code](https://github.com/OpenDriveLab/Openpilot-Deepdive).
- Repos: [commaai/openpilot](https://github.com/commaai/openpilot) (production system), [commaai/comma2k19](https://github.com/commaai/comma2k19) (dataset), community training-pipeline reconstructions like [mbalesni/openpilot-pipeline](https://github.com/mbalesni/openpilot-pipeline).

### B.7 Zoox (Amazon) — cloud-native foundation-model training for robotaxis
- <cite index="96-1">Zoox uses Amazon SageMaker HyperPod to train foundation models for autonomous robotaxis, running multimodal models that process camera, LiDAR, and radar data to handle complex edge cases — combining Hybrid Sharded Data Parallelism and tensor parallelism to achieve 95% GPU utilization across more than 64 GPUs, while ingesting up to 4 TB of vehicle data per hour via AWS Data Transfer.</cite>
- This is a concrete, named example of the "cloud as primary training infrastructure" option in Part A.3 rather than an on-prem/custom-silicon approach like Tesla's Dojo.

### B.8 Cross-cutting pattern across all of them
Every serious player converges on the same four ingredients, just with different emphasis:
1. A **fleet or dedicated-fleet data flywheel** (shadow mode / REM crowdsourcing / dedicated sensor-rich test vehicles)
2. **Auto-labeling + human-review-on-the-hard-subset** to make labeling cost sub-linear in data volume
3. **Simulation/synthetic augmentation** to cover the long tail cheaply (Omniverse NuRec+Cosmos, CARLA, Wayve's GAIA world models, Tesla's own simulator)
4. A **safety-case-aware validation loop** (MIL→SIL→HIL→closed-course→limited public road), even where the underlying model architecture (modular vs. end-to-end vs. world-model/VLA) differs sharply

---

## Consolidated Reference List

**Papers/technical reports**
- Sun et al., *Scalability in Perception for Autonomous Driving: Waymo Open Dataset*, CVPR 2020
- Caesar et al., *nuScenes: A Multimodal Dataset for Autonomous Driving*, CVPR 2020
- Yu et al., *BDD100K: A Diverse Driving Dataset*, CVPR 2020
- Cordts et al., *The Cityscapes Dataset for Semantic Urban Scene Understanding*, CVPR 2016
- Hu et al. (Wayve), *GAIA-1: A Generative World Model for Autonomous Driving*, arXiv:2309.17080, 2023
- Russell et al. (Wayve), *GAIA-2: A Controllable Multi-View Generative World Model for Autonomous Driving*, arXiv:2503.20523, 2025
- Chen et al., *Level 2 Autonomous Driving on a Single Device: Diving into the Devils of Openpilot*, arXiv:2206.08176, 2022
- Kendall et al., *Multi-Task Learning Using Uncertainty to Weigh Losses for Scene Geometry and Semantics*, CVPR 2018
- (Full architecture/task papers — BEVFormer, BEVFusion, DETR3D, YOLO, ByteTrack, etc. — are listed in the companion ADAS pipeline guide from earlier in this conversation.)

**Company technical/blog sources cited above**: Tesla AI Day materials and engineering blog analyses; Waymo Open Dataset site and GitHub; Mobileye technology pages and Justia patent listings; NVIDIA DRIVE/Hyperion newsroom and developer pages; Wayve research archive; comma.ai openpilot GitHub and Wikipedia; AWS architecture blog and re:Invent 2025 recap — full URLs are inline above as markdown links or visible in the citations.

**Patent search starting points** (verify directly rather than trusting any third-party summary):
- [Mobileye Vision Technologies patents — Justia](https://patents.justia.com/assignee/mobileye-vision-technologies-ltd)
- [Google Patents](https://patents.google.com/) filtered to CPC **G06V 20/58** (vehicle scene analysis), **B60W 30/** (vehicle control), assignee = NVIDIA / Qualcomm / Mobileye / Tesla / Waymo as relevant to your competitive question

---

### What would sharpen this further
- If you tell me which competitor(s) you're actually benchmarking against for your ADAS program, and your target SoC/cloud vendor, I can cut this down from "landscape" to a specific, load-bearing build plan (exact toolchain versions, exact dataset mix, exact export path).
- If you want, I can also pull this into a slide deck or Word doc for internal circulation — just say the word.
