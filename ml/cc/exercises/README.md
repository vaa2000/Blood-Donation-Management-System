# Driver Monitoring Systems (DMS) in ADAS — Competitor Architecture Deep-Dive

*A detailed, per-company technical breakdown of computer-vision pipelines, model architectures (CNN/YOLO/Transformer/temporal), training pipelines, deployment stacks, and reference repository structures — with citations to public sources, patents, and research papers.*

---

## 1. What a DMS Does (Shared Problem Definition Across the Industry)

Every commercial Driver Monitoring System solves roughly the same perception problem, regardless of vendor:

1. **Sense** the driver's face/body/eyes with a near-infrared (NIR) camera (850nm/940nm) so it works day and night without visible glare.
2. **Detect & localize** the face, facial landmarks, eyes, iris/pupil, head pose, and body/hands.
3. **Infer driver state**: gaze direction/zone, eyelid aperture (PERCLOS), blink rate, yawning, head nod/droop, phone use, seatbelt, smoking, distraction/emotion.
4. **Temporally integrate** these frame-level cues into drowsiness/distraction *scores* using state machines or temporal deep nets (avoiding single-frame false positives).
5. **Trigger interventions** (visual/audio/haptic alerts, escalation to Level‑2/3 hand-back requests) in real time (typically 30–60 fps, <100ms latency) on an embedded automotive SoC that is ASIL-B/ASIL-D functional-safety qualified.

This is driven by regulation: the **EU General Safety Regulation (GSR) 2019/2144** and **Euro NCAP's 2023–2026 roadmap** made driver drowsiness/distraction warning systems mandatory for new type-approvals, which is why nearly every Tier 1 and semiconductor vendor now has a DMS product.

---

## 2. Generic Reference Architecture (Applies to Nearly All Vendors)

```
┌────────────────────────────────────────────────────────────────────┐
│                         SENSING LAYER                               │
│  NIR camera (RGB-IR / IR-only) + IR LED illuminator (850/940nm)     │
│  Optional: ToF/depth camera, radar (occupancy), steering-wheel      │
│  torque/grip sensors, seat pressure, biometric wearables            │
└───────────────────────────────┬──────────────────────────────────────┘
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│                    PRE-PROCESSING / ISP LAYER                       │
│  Auto-exposure for IR, denoise, lens undistortion, ROI crop,        │
│  frame sync (multi-camera), region normalization                    │
└───────────────────────────────┬──────────────────────────────────────┘
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│              STAGE 1: FACE / PERSON DETECTION                       │
│  Lightweight detector: YOLO-family (YOLOv4/v5/v7-tiny), SSD-Mobile- │
│  Net, RetinaFace, or BlazeFace-style anchor-free detectors           │
└───────────────────────────────┬──────────────────────────────────────┘
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│      STAGE 2: LANDMARK / HEAD POSE / EYE-REGION CNN                 │
│  Facial landmark CNN (68/98-pt or dense heatmap regression),         │
│  head-pose regression (Euler angles / 6DoF), eye-crop CNN for       │
│  eyelid aperture, iris/pupil localization                            │
└───────────────────────────────┬──────────────────────────────────────┘
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│      STAGE 3: GAZE ESTIMATION / ACTION RECOGNITION                   │
│  Appearance-based gaze CNN (eye+face+landmark fusion), gaze-zone     │
│  classifier; secondary CNN/ViT branch for driver activity (phone,   │
│  smoking, hands-off-wheel) — often a YOLO-style object detector      │
│  for phone/cup/cigarette objects near hands/face                    │
└───────────────────────────────┬──────────────────────────────────────┘
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│      STAGE 4: TEMPORAL / SEQUENCE MODEL                              │
│  Options used industry-wide: rule-based state machines (PERCLOS,    │
│  blink-rate thresholds), LSTM/GRU/ConvLSTM over feature sequences,   │
│  1D-CNN/TCN, or Transformer encoders (TransDARC, M2DAR, Video        │
│  Swin/VideoMAE-style spatio-temporal transformers) for higher-order  │
│  activity recognition and drowsiness-level regression                │
└───────────────────────────────┬──────────────────────────────────────┘
                                 ▼
┌────────────────────────────────────────────────────────────────────┐
│      STAGE 5: DECISION / ALERT / FUNCTIONAL-SAFETY LAYER             │
│  Confidence fusion, hysteresis, ASIL-rated monitor, CAN/Ethernet      │
│  signal output to cluster/HMI, escalation logic for L2+/L3 hand-back│
└──────────────────────────────────────────────────────────────────────┘
```

### Generic ML repo / MLOps structure used across the industry

```
dms-perception/
├── data/
│   ├── raw/                     # camera captures, per-subject, per-session
│   ├── annotations/             # landmarks, gaze vectors, activity labels (OpenLABEL/VCD, COCO-style)
│   ├── synthetic/                # simulator-rendered (Omniverse/CARLA/Unity) faces & cabins
│   └── splits/                   # train/val/test, subject-independent splits
├── src/
│   ├── detection/                # face/person detector training (YOLO/RetinaFace configs)
│   ├── landmarks/                 # landmark + head-pose CNN
│   ├── gaze/                      # gaze regression network
│   ├── activity/                  # distraction/action recognition (CNN-LSTM / transformer)
│   ├── temporal/                  # sequence models, state machines
│   └── fusion/                    # multi-modal fusion, decision layer
├── training/
│   ├── configs/                   # hyperparameters, augmentation policies
│   ├── train.py, eval.py
│   └── experiment_tracking/       # MLflow / Weights&Biases integration
├── export/
│   ├── onnx_export.py
│   ├── quantization/               # INT8/PTQ/QAT calibration scripts
│   └── compiler_backends/          # TensorRT, Ambarella CVflow SDK, Qualcomm SNPE/QNN, TI TIDL, NXP eIQ
├── deployment/
│   ├── ecu_runtime/                 # AUTOSAR Adaptive / QNX / embedded Linux integration
│   ├── safety_monitor/              # ASIL plausibility checks, watchdogs
│   └── ota/                          # over-the-air model update packaging
├── validation/
│   ├── hil_bench/                    # hardware-in-the-loop test rigs
│   ├── scenario_replay/               # recorded edge cases (sunglasses, masks, low light)
│   └── metrics/                       # PERCLOS accuracy, gaze RMSE, FAR/FRR
└── ci_cd/
    └── pipelines.yaml                  # automated retrain-on-drift, regression gating
```

This generic structure is a synthesis of publicly described practices (Ambarella's CNN toolkit/compiler-debugger flow for Caffe/TensorFlow/PyTorch models, NVIDIA's DRIVE Replicator synthetic-data + DGX training + DRIVE Sim validation loop, and academic DMS pipeline papers cited below) — no single company publishes its full internal repo layout, but this mirrors what is described in their technical marketing and patents.

---

## 3. Company-by-Company Breakdown

### 3.1 Smart Eye (Sweden) — incl. Affectiva (emotion AI, acquired 2021)

**Position**: Largest pure-play DMS supplier by production vehicle volume; "Human Insight AI."

**Architecture**: Smart Eye's automotive DMS is described as running **10 deep neural networks in parallel** for eye movement, facial expression, body posture, and gesture recognition, which — in partnership with NVIDIA — were accelerated more than 10x on GPU hardware for the DRIVE IX ecosystem. Their stack is explicitly **hardware-agnostic** (deployable on any CPU/SoC) and built to automotive **ASIL** functional-safety levels under **Automotive SPICE** development processes, so the same trained models can be re-targeted across silicon vendors without redesign.

At CES 2026, Smart Eye demonstrated its DMS running as a **mixed-criticality workload** alongside the digital instrument cluster on a single ECU, using Green Hills Software's ASIL-certified **INTEGRITY RTOS**, with driver-distraction events broadcast over the vehicle network (SOA/service-oriented architecture) to trigger cluster HMI alerts — showing a shift toward centralized "software-defined vehicle" compute rather than a dedicated DMS ECU.

**Training/deployment pipeline (as publicly described)**: model development and validation partly leverages NVIDIA's **DRIVE Replicator** synthetic data generation on **DRIVE Sim**, with training on **DGX** systems, and testing on the target embedded SoC via DRIVE IX perception APIs (GazeNet/SleepNet/ActivityNet-style modular DNN blocks — see NVIDIA section for the underlying DRIVE IX taxonomy that partners like Smart Eye build on).

**Emotion AI (Affectiva heritage)**: CNN-based facial-action-unit and emotion classifiers layered on top of the geometric driver-state pipeline, feeding cognitive-load/mood inference for occupant experience personalization.

**References**:
- Smart Eye DMS product page: https://smarteye.se/solutions/automotive/driver-monitoring-system/
- Smart Eye × Green Hills CES 2026 architecture demo: https://www.ghs.com/news/20260105_ces_smarteye_driver_monitoring.html
- NVIDIA Blog — "Inside AI: NVIDIA DRIVE Ecosystem… DRIVE IX" (Smart Eye's 10-DNN pipeline, 10x GPU speedup): https://blogs.nvidia.com/blog/drive-ix-ecosystem/

---

### 3.2 Seeing Machines (Australia) — FOVIO / e-DME / Occula NPU

**Position**: 20+ years of human-factors research (Guardian fleet product + FOVIO automotive OEM product); production debut on the 2018 Cadillac CT6.

**Architecture**: The **FOVIO Chip Platform** pairs Seeing Machines' DMS software (including its own **Occula neural processing unit IP**) with commodity silicon (originally Xilinx/AMD Zynq-7000 automotive SoCs). The stack has three named pillars: (1) the **FOVIO Chip** (acceleration IP + DMS software co-packaged as an ASSP), (2) a "low-friction integration pathway" for OEM/Tier-1 adoption, and (3) the **Occula NPU** for embedded neural inference. Their software (branded **e-DME**, "embedded Driver Monitoring Engine") performs head, face, eye, and eyelid tracking plus gaze-vector estimation, built on top of decades of proprietary computer-vision/human-factors research and **naturalistic driving datasets**.

**Deployment flexibility**: The same e-DME software layer is explicitly designed to be **hardware/SoC agnostic** — announced integrations include Ambarella CVflow (via the Cipia/partner ecosystem lineage) and Analog Devices' GMSL SerDes + IR-LED driver hardware for camera connectivity, indicating a modular hardware abstraction layer between the neural inference core and the camera/illumination front end.

**Recent direction**: Partnership with **Valeo** (announced for CES 2026) combining Seeing Machines' perception software with Valeo's system integration — features include gaze-tracking-verified hazard-awareness (Panovision HUD) and multi-SoC portability demos.

**References**:
- Seeing Machines Automotive / FOVIO overview: https://seeingmachines.com/products/automotive/
- FOVIO Chip family page: https://seeingmachines.com/products/automotive/fovio-chip-family/
- "Seeing Machines announces new product strategy" (FOVIO chip / Occula NPU pillars), S&P Global AutoTechInsight: https://autotechinsight.spglobal.com/news/5256875/
- ADI + Seeing Machines hardware partnership (GMSL/IR-LED): https://www.allaboutcircuits.com/news/seeing-machines-snags-numerous-partnerships-to-advance-adas/
- Valeo × Seeing Machines CES 2026: https://www.valeo.com/en/valeo-unveils-safety-enhancing-advanced-monitoring-applications-powered-by-seeing-machines-at-ces-2026/

---

### 3.3 Cipia (Israel, formerly Eyesight Technologies)

**Position**: Mass-market/entry-level DMS specialist; chosen for 40+ vehicle models across 8 OEMs; partnerships with Mobileye, OmniVision, Ambarella, and Arm.

**Architecture (explicitly described in interviews)**: Cipia's pipeline uses **two layers of AI algorithms**:
1. **Layer 1 — Geometric/appearance CNNs**: face detection and head-pose tracking, eyelid-openness estimation, gaze-direction estimation from the video stream.
2. **Layer 2 — Physiological-state translation network**: converts the Layer‑1 facial cues (e.g., a specific pattern of blinks) into a **drowsiness-level score**, essentially a temporal/statistical model sitting on top of the frame-level CNN outputs.

Cipia's DriverSense product also runs object/action detectors for phone-use and seatbelt detection.

**Silicon targets**: Deployed on **OmniVision's OAX8000 ASIC** (entry-level NCAP-compliance tier, using the chip's on-die NPU), **Ambarella CVflow SoCs (CV22/CV25/CV28/CV2)**, and demonstrated running efficiently on plain **Arm Cortex CPUs without a dedicated NPU/accelerator** (2025 Arm/Embedded World demo) — showing Cipia optimizes the same neural architectures for very different compute envelopes (dedicated NPU vs. CPU-only inference), a key differentiator for cost-sensitive/retrofit compliance use cases.

**References**:
- Just Auto Q&A with Cipia (two-layer AI architecture, OmniVision OAX8000 NPU): https://www.just-auto.com/interview/beyond-driver-monitoring-qa-with-cipia/
- Digitimes — Cipia technical explainer (2-layer algorithm description): https://www.digitimes.com/news/a20230822VL206/cipia-israel-ai-driver-monitoring-china.html
- Cipia × Arm CPU-only inference demo (2025): https://www.marketscreener.com/quote/stock/CIPIA-VISION-LTD-129942643/news/Cipia-Collaborates-with-Arm-49227851/
- Cipia × Ambarella CVflow integration (CV25/CV22/CV2, CV28): https://cipia.com/news/eyesight-technologies-driver-sense-driver-monitoring-system-now-available-ambarella-cvflow-ai-socs/
- OmniVision × Cipia OAX8000 partnership: https://www.ovt.com/press-releases/cipia-and-omnivision-partner-to-bring-industrys-first-mass-market-driver-monitoring-solution/

---

### 3.4 Bosch (Germany)

**Position**: Tier‑1 with camera-based DMS integrated into its broader ADAS/interior-sensing portfolio; also a frequent showcase partner for third-party DMS vendors (e.g., Seeing Machines FOVIO demonstrated on a Bosch show car at CES).

**Architecture (from independent research aligned with Bosch-affiliated/industry papers)**: Bosch's published and academically-referenced approach to gaze/head monitoring follows the same detect→landmark→gaze pipeline as the rest of the industry, with recent published research (e.g., the **FaceEyeNet** architecture) using a **YOLOv7-based face/eye-pair detector**, followed by a custom CNN that fuses **Binary Pattern Feature Extraction (BPFE)** and **ORB (Oriented FAST and Rotated BRIEF)** hand-crafted features with learned CNN features for head-pose and eye-gaze-angle regression — an example of the hybrid "classical CV descriptor + CNN" design pattern still common in production-grade automotive gaze estimation (favored for robustness and lower compute than pure end-to-end deep nets).

Bosch's broader ADAS strategy embeds DMS as one function within its central ADAS/AD domain-control-unit compute stack, alongside forward-facing perception, consistent with the general industry direction toward sensor-fusion domain controllers rather than isolated DMS ECUs.

**References**:
- "AI-enabled driver assistance: monitoring head and gaze movements for enhanced safety" (FaceEyeNet: YOLOv7 + BPFE + ORB + CNN), Complex & Intelligent Systems / Springer: https://link.springer.com/article/10.1007/s40747-025-01897-7
- Seeing Machines FOVIO shown on Bosch ShowCar, CES 2018: https://ai-online.com/2018/01/seeing-machines-to-showcase-fovio-camera-based-driver-monitoring-system-at-ces-2018/
- Global Market Insights industry report (Bosch's sensor-fusion DMS positioning): https://www.gminsights.com/industry-analysis/automotive-driver-monitoring-system-market

---

### 3.5 Continental AG (Germany)

**Position**: Major Tier-1 combining in-cabin camera DMS with its broader ADAS/braking/radar-lidar portfolio; frequently cited alongside Bosch and Denso as a "full-stack" ADAS+DMS integrator.

**Architecture**: Continental's camera-based interior sensing follows the industry-standard face/eye detection → landmark → gaze/eyelid pipeline, integrated into its cross-domain ADAS ECUs so DMS state can be fused directly with forward-collision and lane-keep systems for coordinated Level‑2/2+ safety interventions (single sensor-fusion decision layer rather than isolated DMS alerts). Public market analyses consistently group Continental with Bosch/Denso as the players best positioned to combine in-cabin monitoring with radar/lidar/braking systems for holistic safety responses — reflecting Continental's cross-domain-controller strategy rather than a stand-alone DMS chip play.

**References**:
- Global Market Insights, Automotive DMS Market report (Continental's sensor-fusion classification): https://www.gminsights.com/industry-analysis/automotive-driver-monitoring-system-market
- OpenPR market-competitive-landscape release listing Continental AG: https://www.openpr.com/news/4512733/driver-monitoring-camera-and-sensor-systems-industry-projected

---

### 3.6 Valeo (France)

**Position**: Full-stack Tier‑1 for "Interior Cocoon" — DMS + occupant monitoring + child-presence detection, now powered by its 2024 acquisition of **Asaphus** (AI perception team) and 2026 strategic collaboration with **Seeing Machines**.

**Architecture**: Valeo's **Driver Monitoring System** and **Interior Monitoring System (IMS)** use a wide field-of-view **2D RGB-IR / RGB / RGB-Low-Light camera**, connected to a processing ECU, running AI algorithms that classify age, gender, clothing level, posture, and (for DMS specifically) distraction/drowsiness/facial-emotion state. A separate **interior radar-based occupant monitoring system** complements the camera stack for child/animal presence detection via breathing/motion-pattern analysis — an explicit multi-modal sensor fusion architecture (camera CNN + radar signal-processing classifier) rather than vision-only.

Valeo's own **DMS research contributions** include a heatmap-based cognitive-distraction estimation method that fuses the Valeo DMS's head-position/eye-position/gaze-vector output with a 3D world model of the cabin (mirror/cluster/window positions) to determine which real-world object the driver is looking at — a geometric gaze-mapping layer on top of the core gaze-vector CNN.

**2026 direction**: Valeo is replacing its in-house perception core with Seeing Machines' AI models (a strategic "buy vs. build" shift), while retaining system-level integration, ECU design, and application logic (e.g., Panovision HUD gaze-verified hazard alerts, SmartCluster two-wheeler helmet detection) in-house.

**References**:
- Valeo DMS product page: https://www.valeo.com/en/catalogue/cda/driver-monitoring-system/
- Valeo In-Vehicle Monitoring System (IMS): https://www.valeo.com/en/catalogue/cda/in-vehicle-monitoring-system/
- Valeo interior radar occupant monitoring: https://www.valeo.com/en/catalogue/cda/interior-radar-based-occupant-monitoring-system/
- Valeo × Seeing Machines CES 2026 collaboration + Asaphus acquisition history: https://www.autonomousvehicleinternational.com/news/sensors/valeo-and-seeing-machines-partner-on-driver-and-occupant-monitoring-systems.html
- "Heatmap-Based Method for Estimating Drivers' Cognitive Distraction" (uses Valeo DMS output + 3D cabin model), arXiv: https://arxiv.org/pdf/2005.14136

---

### 3.7 Aptiv (Ireland/USA)

**Position**: Positions DMS/CMS (Cabin Monitoring System) as part of a broader "Smart Vehicle Architecture" and Gen-6 ADAS platform, pushing toward vision-only occupant sensing (replacing seat-weight sensors) and edge-processed cabin AI.

**Architecture**: Aptiv's 2026-generation Cabin Monitoring System is explicitly moving to **vision-based occupancy detection** to fully replace traditional seat-weight sensors, feeding richer, more granular occupant data for airbag-deployment optimization and child-presence detection — implying a segmentation/pose-estimation CNN operating on cabin-wide camera views rather than a per-seat pressure sensor. Their "**Intelligent Edge**" architecture processes in-cabin sensor data locally at the edge for low-latency safety decisions, paired with continuous cloud-side model refinement ("digital feedback loop"). Aptiv also uses **synthetic-data simulation environments** to generate the large, balanced datasets needed to train and validate its in-cabin AI models, addressing the classic long-tail/rare-scenario data problem in DMS training (e.g., unusual postures, occlusions, diverse demographics).

**References**:
- "CES 2026 In-Cabin Monitoring: From DMS to Agentic Intelligence" (Aptiv's synthetic data + Intelligent Edge + vision-based occupancy): https://anyverse.ai/in-cabin-monitoring-ces-2026/
- Spherical Insights market report (Aptiv Gen-6 ADAS AI/ML platform): https://www.sphericalinsights.com/press-release/automotive-driver-monitoring-system-market

---

### 3.8 Magna International (Canada)

**Position**: Integrates DMS/OMS camera + IR emitters + ECU directly into the **frameless interior rear-view mirror** for minimal packaging footprint; secured high-volume OEM orders (e.g., a German automaker) for this design.

**Architecture**: Magna's system fully integrates a high-resolution camera, IR emitters, and the processing ECU **inside the mirror housing**, with the camera hidden behind the mirror glass for an unobstructed driver view and minimal styling impact. Software tracks head, eye, and body movement to output distraction/drowsiness/fatigue alerts, and the platform is explicitly **scalable** to child-presence detection, seatbelt detection, video conferencing, and facial-recognition-based personalization — a single hardware module serving multiple downstream ML tasks. Magna has also announced a strategic collaboration with **NVIDIA** to integrate the next-gen **DRIVE AGX Thor** SoC for combined L2+–L4 ADAS, interior sensing, and cabin-AI workloads on one compute platform (echoing the industry-wide consolidation of DMS into central domain/zone controllers).

**References**:
- Magna DMS product page (mirror-integrated architecture): https://www.magna.com/products/exterior-interior/mirrors/driver-monitoring-system
- Magna OEM order announcement, ADAS & Autonomous Vehicle International: https://www.autonomousvehicleinternational.com/news/safety/magna-secures-oem-order-for-integrated-occupant-monitoring-system.html
- Magna 40-F FY2025 SEC filing (NVIDIA DRIVE AGX Thor collaboration for interior sensing/cabin-AI): https://www.sec.gov/Archives/edgar/data/749098/000119312526128771/d20215dex991.htm

---

### 3.9 Denso (Japan)

**Position**: Grouped consistently with Bosch/Continental as a top-tier full-stack DMS+ADAS integrator, leveraging its position as a Toyota-affiliated Tier‑1 with deep camera/radar sensor-fusion capability.

**Architecture**: Denso's public positioning emphasizes combining in-cabin monitoring with its existing radar/LiDAR/braking-system portfolio for coordinated safety responses (same general fusion architecture pattern as Bosch/Continental), targeting compliance with Euro NCAP 2025 roadmap and NHTSA mandates. As with several peers, detailed internal model architecture is not publicly disclosed at the same technical depth as Seeing Machines/Cipia/NVIDIA; Denso's differentiation is emphasized in market analyses as **regulatory-compliance speed and vehicle-platform integration** rather than published novel network architectures.

**References**:
- Global Market Insights DMS report (Denso positioning): https://www.gminsights.com/industry-analysis/automotive-driver-monitoring-system-market
- Spherical Insights market vendor list including Denso: https://www.sphericalinsights.com/press-release/automotive-driver-monitoring-system-market

---

### 3.10 NVIDIA — DRIVE IX Platform (Enabling Platform Used by Many of the Above)

**Position**: Not itself a DMS product vendor to consumers, but the dominant **compute + software platform** underneath many OEM/Tier-1 DMS deployments (Smart Eye, Magna, and others build on DRIVE IX / DRIVE AGX).

**Architecture (explicitly documented by NVIDIA)**: DRIVE IX runs **multiple purpose-built DNNs in a pipeline**:
- A **face-detection DNN** locates the face in-frame.
- A second DNN identifies **facial fiducial points** (eyes, nose, mouth landmarks).
- **GazeNet**: tracks the eye-gaze vector and maps it onto the road/cabin scene to check whether the driver has seen a given hazard/obstacle.
- **SleepNet**: classifies eyes open/closed and runs the result through a **state machine** to estimate drowsiness/exhaustion level (an explicit rule-based temporal layer on top of the CNN classifier).
- **ActivityNet**: tracks higher-level driver activity — phone usage, hands on/off wheel, attention to road events — plus seating posture.

**Training/deployment pipeline**: NVIDIA's documented workflow is a closed loop: **DGX systems** for large-scale DNN training/optimization → **DRIVE Constellation / DRIVE Sim** virtual proving grounds (near-infinite synthetic driving/cabin conditions) to test and validate the same DNNs that will run on the target embedded hardware → deployment on **DRIVE AGX** (Orin/Thor) under **DRIVE OS** (safety-certified) with **DriveWorks** middleware providing detection, sensor-fusion, and visualization libraries → **DRIVE Replicator** for synthetic in-cabin data generation specifically for interior-sensing partners to bootstrap their DMS training sets.

**References**:
- NVIDIA Blog — "NVIDIA DRIVE IX Keeps Drivers Focused on the Road Ahead" (GazeNet/SleepNet/ActivityNet architecture): https://blogs.nvidia.com/blog/drive-ix-ai-software-drivers-safe/
- NVIDIA Blog — DRIVE IX ecosystem (DRIVE Replicator synthetic data, Smart Eye 10-DNN pipeline): https://blogs.nvidia.com/blog/drive-ix-ecosystem/
- NVIDIA DRIVE Software stack overview (DGX training, DRIVE Sim/Constellation validation loop, DriveWorks SDK): https://www.nvidia.com/en-gb/self-driving-cars/drive-platform/software/
- NVIDIA In-Vehicle Computing / DRIVE AGX Thor: https://developer.nvidia.com/drive/drive-ix
- DriveWorks SDK layered architecture diagram, reproduced in academic survey: https://arxiv.org/pdf/2201.02893

---

### 3.11 Ambarella / Xperi (FotoNation) — Silicon + Vision-IP Layer

**Position**: Ambarella supplies the dominant **CVflow** family of automotive vision SoCs used by Cipia, and others, for DMS inference; Xperi's FotoNation vision IP is a common licensed building block for face/eye detection in embedded cameras industry-wide.

**Architecture**: Ambarella's **CVflow** is a dedicated vision-processing engine (distinct from general CPU/GPU cores) programmed via a high-level algorithm description, letting customers **map their own CNNs — trained in Caffe, TensorFlow, PyTorch, or ONNX — directly onto the chip** using Ambarella's CV toolkit (compiler + debugger + CNN performance-optimization guidelines). Automotive-grade variants (**CV2FS, CV22FS** — ASIL B/C compliant; **CV25, CV28M, CV22, CV2** for cost-optimized DMS) target forward ADAS, DMS, electronic mirrors, and occupancy monitoring, sharing a common SDK across the whole CVflow family so a DMS vendor's trained model can be re-targeted across chip tiers with minimal rework. The newer **CV3-AD** family adds full domain-controller-class AI compute combining ADAS, DMS, and occupancy monitoring on a single chip.

**Training/deployment pipeline (as documented)**: Model developers train in standard frameworks (Caffe/TensorFlow/PyTorch/ONNX) → Ambarella's CV Toolkit **compiles/quantizes** the CNN graph for the CVflow vision engine → on-chip inference runs alongside Ambarella's ISP and video encode pipeline → the same toolchain/SDK is reused for CV22→CV25→CV28→CV3-AD chip-tier ports.

**References**:
- Ambarella CVflow architecture explainer, Macnica: https://www.macnica.com/americas/mai/en/products/semiconductors/ambarella/
- Ambarella CV25 SoC launch (Caffe/TensorFlow CNN toolkit, DMS target): https://www.edge-ai-vision.com/2019/01/ambarella-introduces-cv25-soc-with-cvflow-computer-vision-to-enable-the-next-generation-of-mainstream-intelligent-cameras/
- Ambarella CV22FS/CV2FS ASIL B automotive SoCs: https://investor.ambarella.com/news-releases/news-release-details/ambarella-announces-cv22fs-and-cv2fs-automotive-camera-socs
- Ambarella Automotive applications page (CV3-AD domain controller): https://www.ambarella.com/applications/automotive/

---

### 3.12 Qualcomm / Arriver (formerly Veoneer) (USA/Sweden)

**Position**: Qualcomm acquired Veoneer's **Arriver** software unit (computer vision + drive policy) to build out its **Snapdragon Ride** ADAS/AD platform, which includes driver-monitoring as one of its perception functions alongside forward perception.

**Architecture**: **Snapdragon Ride Pilot** combines Arriver's vision-perception deep-learning stack (trained on over a million miles of real-world driving data across 100+ countries, supplemented with synthetic data and simulation) with Qualcomm's automotive SoCs to deliver 360° perception — object detection, traffic-sign recognition, lane detection, parking assist, and **driver monitoring** — using a mix of rule-based logic and learned models to predict other drivers' behavior. The platform is explicitly **scalable**: a single-camera low-cost configuration for entry NCAP compliance up to a multi-camera/multi-radar Level-2+ configuration, all sharing the same underlying perception-model architecture and toolchain. In 2026, Qualcomm added **Wayve's AI Driver** as an alternative end-to-end, data-driven driving-intelligence layer that can run on the same Snapdragon Ride SoC + Active Safety software stack, illustrating the platform's move toward supporting both classical modular perception stacks (Arriver) and end-to-end learned driving policies (Wayve) side by side.

**References**:
- Qualcomm/Arriver/BMW Snapdragon Ride Vision Perception software announcement: https://www.veoneer.com/en/press/arrivertm-support-qualcomms-technology-collaboration-bmw-vision-perception-software-automated
- "Automated Driving for All: Snapdragon Ride Pilot" (million-mile training data, synthetic + simulation): https://www.edge-ai-vision.com/2025/09/automated-driving-for-all-snapdragon-ride-pilot-system-brings-state-of-the-art-safety-and-comfort-features-to-drivers-across-the-globe/
- Qualcomm × Wayve AI Driver end-to-end stack announcement (2026): https://www.qualcomm.com/news/releases/2026/03/qualcomm-and-wayve-advance-production-ready----end-to-end-ai-for
- Neowin — Snapdragon Ride Pilot / BMW iX3 (driver monitoring as part of 360° perception): https://www.neowin.net/news/qualcomm-readies-new-snapdragon-powered-automated-driving-system-with-bmw/

---

### 3.13 Tobii AB (Sweden) — Eye-Tracking IP Layer

**Position**: Not a Tier‑1 DMS integrator itself, but licenses core **eye-tracking / gaze-estimation** technology (Tobii Autosense) that underlies gaze-vector components used by several automotive partners; listed as a named vendor in market analyses alongside Bosch, Denso, Valeo, etc.

**Architecture**: Tobii's automotive gaze pipeline follows the classic eye-region CNN + iris/pupil localization + appearance-based gaze-regression pattern common to the field's academic literature (see Section 4 datasets/papers below, several of which explicitly benchmark against Tobii-style eye-tracker ground truth, e.g., the DR(eye)VE and Look-Both-Ways datasets used wearable eye trackers as label sources).

**Reference**:
- Automotive Driver Monitoring Market Outlook (Tobii listed as a core vendor): https://www.sphericalinsights.com/press-release/automotive-driver-monitoring-system-market

---

## 4. Cross-Industry Model Zoo: CNN / YOLO / Transformer / Temporal Choices Actually Used (with papers)

| Task | Architectures reported in production/near-production literature | Example papers |
|---|---|---|
| Face/person detection | YOLOv4/v5/v7(-tiny), RetinaFace, SSD-MobileNet, BlazeFace-style anchor-free nets | FaceEyeNet uses **YOLOv7** for face/eye-pair detection (Springer, 2025) |
| Landmark/head-pose | CNN regression (ResNet/MobileNet backbones), classical descriptor fusion (ORB, BPFE) | FaceEyeNet (BPFE+ORB+CNN fusion); POSEidon face-from-depth (CVPR 2017) |
| Gaze estimation | Lightweight appearance-based CNNs optimized for low-power/low-quality sensors; InceptionResNet-V2 transfer learning with regression head | "Efficient CNN Implementation for Eye-Gaze Estimation on Low-Power/Low-Quality Consumer Imaging Systems" (FotoNation/SFI-funded, arXiv:1806.10890); "A Driver Gaze Estimation Method Based on Deep Learning" — YOLOv4 face detector + InceptionResNet-v2 regression (MDPI *Sensors* 2022, PMC9142909) |
| Drowsiness/eye-state | CNN classifiers (EfficientNetB0+ResNet50 hybrids), Vision Transformer & Swin Transformer benchmarked against CNN transfer-learning baselines | "Real-time driver drowsiness detection using transformer architectures" — ViT/Swin vs. VGG19/DenseNet169/ResNet50V2/InceptionV3/MobileNet (*Scientific Reports* 2025); EffRes-DrowsyNet hybrid EfficientNetB0+ResNet50 (PMC12197288) |
| Temporal/sequence modeling | Rule-based state machines (PERCLOS), LSTM/GRU, ConvLSTM, hybrid CNN-LSTM, 1D-CNN/TCN, Transformer encoders | Hybrid CNN+ConvLSTM (AIP Advances, 2026); Long-term Multi-granularity Deep Framework, LSTM-based (arXiv:1801.02325); "Condition-Adaptive Representation Learning" (arXiv:1910.09722) |
| Distraction / activity recognition (multi-view) | Vision Transformers for multi-view/multi-scale fusion; latent-space calibrated transformers | **M2DAR**: Multi-view multi-scale driver action recognition with Vision Transformer; **TransDARC**: Transformer-based driver activity recognition with latent-space feature calibration (IROS 2022) — both cited in arXiv:2503.04078 |
| Object-in-hand detection (phone/cigarette) | YOLO-family object detectors applied to hand/face ROI | Common industry pattern per Cipia/Valeo product descriptions of "phone use / seatbelt" detection |
| Video-level self-supervised backbones | VideoMAE v2 (masked video autoencoder, scalable spatio-temporal transformer) used as a modern backbone candidate for activity recognition research | Cited in "Spatial-Temporal Perception with Causal Inference for Naturalistic Driving Action Recognition" (arXiv:2503.04078) |

**Key research references (arXiv/journal, with URLs):**
- Efficient CNN for low-power gaze estimation (funded by FotoNation/SFI): https://arxiv.org/pdf/1806.10890
- A Driver Gaze Estimation Method Based on Deep Learning (YOLOv4 + InceptionResNet-v2): https://pmc.ncbi.nlm.nih.gov/articles/PMC9142909/
- Faster R-CNN + geometric transform for multi-NIR-camera eye detection: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6338982/
- Driver Monitoring System Based on CNN Models (Springer, attention-level detection): https://link.springer.com/chapter/10.1007/978-3-030-62365-4_56
- Real-time driver drowsiness detection using Transformer architectures (ViT/Swin): https://www.nature.com/articles/s41598-025-02111-x
- Hybrid CNN+ConvLSTM real-time driver safety framework: https://pubs.aip.org/aip/adv/article/16/1/015024/3377521/
- "Intelligent driver monitoring systems" survey (CNN/RNN/LSTM/Transformer taxonomy, PRISMA review 2020–2025): https://link.springer.com/article/10.1007/s10462-026-11505-w
- "Using Visual and Vehicular Sensors for Driver Behavior Analysis: A Survey" (TransDARC, M2DAR references): https://arxiv.org/pdf/2308.13406
- Spatial-Temporal Perception with Causal Inference for Naturalistic Driving Action Recognition (VideoMAE v2, TSM, TransDARC): https://arxiv.org/pdf/2503.04078
- Survey of Transformer architectures for autonomous driving (ScienceDirect, 2025): https://www.sciencedirect.com/science/article/pii/S0957417425039533
- Deep learning for distracted driving recognition, comprehensive review (datasets + models): https://www.sciencedirect.com/science/article/abs/pii/S1383762126000482

---

## 5. Standard Public Datasets Used to Train/Benchmark These Systems

| Dataset | Scale / Content | Notes |
|---|---|---|
| **DMD** (Driver Monitoring Dataset, Vicomtech) | 41h RGB+Depth+IR, 3 synchronized cameras (face/body/hands), 37 drivers | Largest open multi-modal DMS dataset; used for distraction/gaze/drowsiness/hands-wheel research. GitHub: https://github.com/Vicomtech/DMD-Driver-Monitoring-Dataset ; paper: https://link.springer.com/chapter/10.1007/978-3-030-66823-5_23 |
| **NTHU-DDD** | 66,521 images / 36 subjects, IR video, ACCV2016 challenge | Most-cited academic drowsiness benchmark |
| **YawDD** | 107 subjects, RGB video | Yawning detection |
| **DROZY** | 14 subjects, EEG/EOG/EKG/EMG/NIR | Physiological drowsiness |
| **RLDD** | 60 subjects, 30h RGB | Real-life drowsiness |
| **3MDAD** | 50 subjects, RGB/IR/Depth | Distraction |
| **MRL Eye** | 37 subjects, 15,000 IR eye images | Eye-state classification |
| **DADA-2000** | 2,000 accident-scenario videos | Driver attention & accident prediction |
| **Drive&Act** | 12h, 83 activity classes, 6 views, RGB/IR/Depth/3D pose | Fine-grained activity recognition |
| **DR(eye)VE / LBW / DGAZE** | Wearable-eye-tracker-labeled gaze datasets | Ground truth for gaze-vector model validation |

Reference survey covering all of the above with citations: https://www.sciencedirect.com/science/article/abs/pii/S1383762126000482 and dataset table: https://arxiv.org/pdf/2507.13403

---

## 6. Functional-Safety & Deployment Considerations Common Across All Vendors

- **ASIL rating** (ISO 26262): DMS perception chains are increasingly required to meet **ASIL B**, with drowsiness/distraction *warning generation* logic sometimes requiring higher integrity depending on the OEM's hazard analysis. Ambarella's CV2FS/CV22FS chips are explicitly ASIL B/C-compliant silicon for this reason; Smart Eye develops "according to Automotive SPICE processes" for the same reason.
- **Mixed-criticality consolidation**: The 2026 trend (Smart Eye/Green Hills, Aptiv "Intelligent Edge", NVIDIA DRIVE AGX Thor combining ADAS+cockpit+DMS) is to run the DMS DNN stack as one isolated partition on a shared automotive SoC/RTOS (e.g., Green Hills INTEGRITY, QNX) instead of a dedicated DMS ECU — reducing BOM cost while preserving safety isolation via hypervisor/partitioning.
- **Synthetic data pipelines**: NVIDIA DRIVE Replicator/DRIVE Sim, and Aptiv's in-house simulation environments, are explicitly used to generate rare/edge-case cabin scenarios (extreme demographics, occlusion, lighting) that are hard or unsafe to capture naturalistically, then blended with real recorded data for training.
- **On-device quantization/compilation toolchains**: Ambarella CV Toolkit (Caffe/TensorFlow/PyTorch/ONNX → CVflow), Qualcomm SNPE/QNN, NVIDIA TensorRT via DriveWorks — all follow the same pattern: train in a standard framework, export to ONNX, quantize (PTQ/QAT to INT8), compile to the vendor's NPU/vision-DSP instruction set, validate against hardware-in-the-loop rigs.

---

## 7. Consolidated Reference List (All Sources Used)

1. Smart Eye DMS: https://smarteye.se/solutions/automotive/driver-monitoring-system/
2. Smart Eye × Green Hills CES 2026: https://www.ghs.com/news/20260105_ces_smarteye_driver_monitoring.html
3. NVIDIA Blog, DRIVE IX ecosystem (Smart Eye 10-DNN pipeline): https://blogs.nvidia.com/blog/drive-ix-ecosystem/
4. NVIDIA Blog, DRIVE IX architecture (GazeNet/SleepNet/ActivityNet): https://blogs.nvidia.com/blog/drive-ix-ai-software-drivers-safe/
5. NVIDIA DRIVE Software stack: https://www.nvidia.com/en-gb/self-driving-cars/drive-platform/software/
6. NVIDIA DRIVE AGX / DRIVE IX developer page: https://developer.nvidia.com/drive/drive-ix
7. DriveWorks SDK architecture (academic survey reproduction): https://arxiv.org/pdf/2201.02893
8. Seeing Machines Automotive/FOVIO: https://seeingmachines.com/products/automotive/
9. Seeing Machines FOVIO Chip family: https://seeingmachines.com/products/automotive/fovio-chip-family/
10. Seeing Machines product-strategy (Occula NPU): https://autotechinsight.spglobal.com/news/5256875/
11. Seeing Machines × ADI hardware partnership: https://www.allaboutcircuits.com/news/seeing-machines-snags-numerous-partnerships-to-advance-adas/
12. Valeo × Seeing Machines CES 2026: https://www.valeo.com/en/valeo-unveils-safety-enhancing-advanced-monitoring-applications-powered-by-seeing-machines-at-ces-2026/
13. Cipia Q&A (2-layer AI, OmniVision NPU): https://www.just-auto.com/interview/beyond-driver-monitoring-qa-with-cipia/
14. Cipia Digitimes explainer: https://www.digitimes.com/news/a20230822VL206/cipia-israel-ai-driver-monitoring-china.html
15. Cipia × Arm CPU-only demo: https://www.marketscreener.com/quote/stock/CIPIA-VISION-LTD-129942643/news/Cipia-Collaborates-with-Arm-49227851/
16. Cipia × Ambarella CVflow: https://cipia.com/news/eyesight-technologies-driver-sense-driver-monitoring-system-now-available-ambarella-cvflow-ai-socs/
17. Cipia × OmniVision OAX8000: https://www.ovt.com/press-releases/cipia-and-omnivision-partner-to-bring-industrys-first-mass-market-driver-monitoring-solution/
18. FaceEyeNet (Bosch-relevant YOLOv7+BPFE+ORB): https://link.springer.com/article/10.1007/s40747-025-01897-7
19. Valeo DMS: https://www.valeo.com/en/catalogue/cda/driver-monitoring-system/
20. Valeo IMS: https://www.valeo.com/en/catalogue/cda/in-vehicle-monitoring-system/
21. Valeo interior radar OMS: https://www.valeo.com/en/catalogue/cda/interior-radar-based-occupant-monitoring-system/
22. Valeo heatmap cognitive-distraction paper (arXiv): https://arxiv.org/pdf/2005.14136
23. Aptiv CES 2026 in-cabin monitoring: https://anyverse.ai/in-cabin-monitoring-ces-2026/
24. Magna DMS mirror-integrated: https://www.magna.com/products/exterior-interior/mirrors/driver-monitoring-system
25. Magna OEM order news: https://www.autonomousvehicleinternational.com/news/safety/magna-secures-oem-order-for-integrated-occupant-monitoring-system.html
26. Magna 40-F FY2025 (NVIDIA DRIVE AGX Thor): https://www.sec.gov/Archives/edgar/data/749098/000119312526128771/d20215dex991.htm
27. Ambarella CVflow overview: https://www.macnica.com/americas/mai/en/products/semiconductors/ambarella/
28. Ambarella CV25 launch: https://www.edge-ai-vision.com/2019/01/ambarella-introduces-cv25-soc-with-cvflow-computer-vision-to-enable-the-next-generation-of-mainstream-intelligent-cameras/
29. Ambarella CV22FS/CV2FS ASIL: https://investor.ambarella.com/news-releases/news-release-details/ambarella-announces-cv22fs-and-cv2fs-automotive-camera-socs
30. Ambarella automotive/CV3-AD: https://www.ambarella.com/applications/automotive/
31. Qualcomm/Arriver/BMW Snapdragon Ride: https://www.veoneer.com/en/press/arrivertm-support-qualcomms-technology-collaboration-bmw-vision-perception-software-automated
32. Snapdragon Ride Pilot (million-mile training data): https://www.edge-ai-vision.com/2025/09/automated-driving-for-all-snapdragon-ride-pilot-system-brings-state-of-the-art-safety-and-comfort-features-to-drivers-across-the-globe/
33. Qualcomm × Wayve (2026): https://www.qualcomm.com/news/releases/2026/03/qualcomm-and-wayve-advance-production-ready----end-to-end-ai-for
34. Global Market Insights DMS market report (Bosch/Continental/Denso positioning): https://www.gminsights.com/industry-analysis/automotive-driver-monitoring-system-market
35. Spherical Insights DMS market outlook (vendor list, Tobii etc.): https://www.sphericalinsights.com/press-release/automotive-driver-monitoring-system-market
36. Efficient CNN for low-power gaze estimation: https://arxiv.org/pdf/1806.10890
37. Driver Gaze Estimation via Deep Learning (YOLOv4 + InceptionResNet-v2): https://pmc.ncbi.nlm.nih.gov/articles/PMC9142909/
38. Faster R-CNN + geometric transform NIR eye detection: https://www.ncbi.nlm.nih.gov/pmc/articles/PMC6338982/
39. DMS based on CNN models (attention-level detection): https://link.springer.com/chapter/10.1007/978-3-030-62365-4_56
40. Transformer-based real-time drowsiness detection (ViT/Swin): https://www.nature.com/articles/s41598-025-02111-x
41. Hybrid CNN+ConvLSTM driver safety framework: https://pubs.aip.org/aip/adv/article/16/1/015024/3377521/
42. Intelligent driver monitoring systems survey (PRISMA 2020-2025): https://link.springer.com/article/10.1007/s10462-026-11505-w
43. Visual/vehicular sensor driver-behavior survey (TransDARC, M2DAR): https://arxiv.org/pdf/2308.13406
44. Spatial-Temporal Perception w/ Causal Inference (VideoMAE v2, TSM): https://arxiv.org/pdf/2503.04078
45. Survey of Transformer architectures for autonomous driving: https://www.sciencedirect.com/science/article/pii/S0957417425039533
46. Deep learning for distracted driving recognition (comprehensive review + datasets): https://www.sciencedirect.com/science/article/abs/pii/S1383762126000482
47. DMD dataset paper: https://link.springer.com/chapter/10.1007/978-3-030-66823-5_23
48. DMD GitHub repo: https://github.com/Vicomtech/DMD-Driver-Monitoring-Dataset
49. UL-DD multimodal drowsiness dataset (comparison table of datasets): https://arxiv.org/pdf/2507.13403
50. myEye2Wheeler gaze dataset paper (dataset landscape review): https://arxiv.org/pdf/2502.12723

---

*Note on scope: publicly available sources describe production architecture at varying levels of technical depth — some vendors (NVIDIA, Ambarella, Cipia, Seeing Machines) disclose named model components and toolchains, while others (Denso, Continental, Bosch's proprietary in-house pipeline) primarily disclose system-level positioning rather than internal network architectures in public sources. Where a company's own technical disclosures were limited, this report supplements with the closest independently published/peer-reviewed research most representative of that company's stated approach, explicitly marked as such above.*
