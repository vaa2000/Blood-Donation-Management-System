# Driver Monitoring Systems (DMS) in ADAS
# Deep-Dive Architecture Document
### Hardware · Software · Computer-Vision Pipeline · Training/Testing/Deployment · Communication Stack · Patents · Papers · Open Source

---

## This document includes **every company** analysis  :

1. Detailed architecture — hardware, software, computer vision (+ where this company sits in the common industry pattern).
2. End-to-end CV pipeline — creation → training → deployment; concrete model families (CNN/YOLO/ViT/temporal) with references.
3. Cameras used, positions, how the DMS is developed/executed, full block-diagram architecture, and every communication component (CAN, Ethernet, AUTOSAR, SOME/IP, DDS; LiDAR noted as N/A for cabin-facing DMS — see Section 0.4).
4. AI training pipeline and deployment pipeline, in detail.
5. Patents, research papers, conference publications, and open-source implementations mapped to each pipeline stage.

Every factual claim about a **named company** is drawn from that company's own public technical/product pages, patents, or press/interview material — all cited inline. Claims about **architecture patterns in general** draw on the peer-reviewed/arXiv literature (Section 0.5) and are explicitly flagged as **industry-representative** rather than a confirmed statement about a specific company's shipped source code, since no company publishes exact production model internals (trade secret). Section 0.6 repeats this caveat — read it before treating any diagram below as literal shipped code.

---

# PART 0 — Shared Foundations (applies to every company in Part 1 onward)

## 0.1 Regulatory drivers that shaped every architecture below
- **UN Regulation No. 158** (Driver Drowsiness and Attention Warning, DDAW) and **No. 159** (Advanced Driver Distraction Warning, ADDW).
- **EU General Safety Regulation (GSR) 2019/2144** — DDAW/ADDW mandatory for new type-approvals from **July 2022**, all new vehicles from **July 2024**.
- **Euro NCAP** Safety Assist protocol (2020 roadmap onward) scores DMS as part of the star rating; **Tobii's own site states "All new cars in the EU will require a camera-based DMS by the year 2026."**
- **ISO 26262** (functional safety, ASIL decomposition) and **ISO 21448 / SOTIF** (safety of the intended function — critical because DMS is a perception-ML system whose failure modes are probabilistic, not purely deterministic).

## 0.2 The common reference architecture (pattern shared by every implementor covered)

```
 ┌───────────────────────────────────────────────────────────────────────┐
 │ 1. CABIN SENSING HARDWARE                                              │
 │    NIR (850/940 nm) camera + IR-LED illuminator ring                  │
 │    (+ optional wide-FOV RGB/RGB-IR for OMS, + optional cabin radar)   │
 │    Mount point: steering column / A-pillar / rear-view-mirror housing/ │
 │    center dashboard / overhead console — company-specific, see below │
 └───────────────────────────────┬─────────────────────────────────────--┘
                                 │ GMSL2 / FPD-Link III coax SerDes, or MIPI-CSI2
                                 ▼
 ┌───────────────────────────────────────────────────────────────────────┐
 │ 2. IMAGE SIGNAL PROCESSING (ISP)                                       │
 │    De-Bayer, AGC/AEC tuned for IR, HDR fusion, IR-LED flicker         │
 │    mitigation, eye-safety irradiance limiting, lens-shading correction│
 └───────────────────────────────┬─────────────────────────────────────--┘
                                 ▼
 ┌───────────────────────────────────────────────────────────────────────┐
 │ 3. PERCEPTION / CV LAYER (runs on NPU / GPU / DSP inside SoC or ECU)   │
 │    a. Face detection + tracking       (CNN / YOLO-family / SSD)       │
 │    b. Facial landmarking (68–468 pt)  (CNN or ViT)                    │
 │    c. Head-pose regression (6-DoF)                                    │
 │    d. Eye/iris/pupil analysis → gaze vector, blink, PERCLOS           │
 │    e. Mouth/yawn analysis (MAR)                                       │
 │    f. Occlusion / sunglasses / mask detection                         │
 │    g. Object-in-hand detection (phone, cigarette, cup) — YOLO head    │
 │    h. (OMS extension) body pose, seatbelt, occupant classification    │
 └───────────────────────────────┬─────────────────────────────────────--┘
                                 ▼
 ┌───────────────────────────────────────────────────────────────────────┐
 │ 4. TEMPORAL / STATE-FUSION LAYER                                       │
 │    PERCLOS + blink-rate sliding window; LSTM/GRU/TCN/Transformer over │
 │    per-frame embeddings for micro-sleep & distraction classification; │
 │    gaze-zone mapping (mirrors/cluster/infotainment/road) via HMM/RNN  │
 └───────────────────────────────┬─────────────────────────────────────--┘
                                 ▼
 ┌───────────────────────────────────────────────────────────────────────┐
 │ 5. DECISION / HMI ARBITRATION                                          │
 │    Threshold + hysteresis (ISO 15007 gaze metrics); takeover-readiness│
 │    confidence for L2+/L3; escalation ladder: visual→audio→haptic→MRM  │
 └───────────────────────────────┬─────────────────────────────────────--┘
                                 ▼
 ┌───────────────────────────────────────────────────────────────────────┐
 │ 6. VEHICLE NETWORK / MIDDLEWARE                                        │
 │    Classic AUTOSAR (CAN/CAN-FD, COM/PduR) for low-bandwidth alerts;   │
 │    Adaptive AUTOSAR (SOME/IP, DDS, DoIP) over Automotive Ethernet for │
 │    zonal/HPC video + service-oriented state distribution              │
 └───────────────────────────────────────────────────────────────────────┘
```

## 0.3 Camera-position taxonomy used across the industry (referenced repeatedly below)
| Mount position | Typical field of view | Used for |
|---|---|---|
| Steering column top / instrument binnacle | Narrow, driver-only | Classic DMS (DENSO's earliest DSM, many Toyota/Lexus programs) |
| A-pillar | Narrow-to-medium, driver-only, resistant to steering-wheel occlusion | Seeing Machines, Smart Eye typical mounting |
| Dashboard / center-console-top | Narrow-medium, driver-only | Valeo DMS, many entry-level programs |
| Rear-view-mirror housing / frameless mirror module | Wide FOV, driver + front passenger + partial rear | Bosch OMC, Magna's mirror-integrated DMS/OMS, Valeo dome-module IMS |
| Overhead console / headliner | Wide FOV camera and/or radar aperture | Bosch cabin-sensing radar, some OMS radar fusion designs |
| Combined front-camera module (behind windshield) | Forward + a second cabin-facing sensor in the same housing | Continental's combined front+interior camera; Mobileye's single-EyeQ-chip DMS+ADAS fusion |

## 0.4 Note on LiDAR
No DMS/interior-sensing implementation reviewed uses **LiDAR** for the driver-facing sensing function — LiDAR is an *exterior* perception sensor (used for forward/surround ADAS by, e.g., Continental+Ambarella CV3-AD's sensor fusion, or Valeo's L3 LiDAR on the Mercedes S-Class/Honda Legend). Interior sensing instead uses **NIR cameras**, optionally supplemented by **short-range cabin radar** (Bosch) for vital-sign/occlusion-robust occupancy sensing. This document includes LiDAR only where it appears in a company's *broader* ADAS sensor suite (noted explicitly), not in the DMS pipeline itself.

## 0.5 Master reference list — architectures, datasets, and general literature (cited by shorthand in every company section)
- **[R1]** Ranjan, Patel, Chellappa, "HyperFace: A Deep Multi-Task Learning Framework for Face Detection, Landmark Localization, Pose Estimation, and Gender Recognition," IEEE TPAMI.
- **[R2]** Zhang, Sugano, Fritz, Bulling, "MPIIGaze: Real-World Dataset and Deep Appearance-Based Gaze Estimation," IEEE TPAMI 41(1):162–175, 2017.
- **[R3]** Kellnhofer, Recasens, Stent, Matusik, Torralba, "Gaze360: Physically Unconstrained Gaze Estimation in the Wild," ICCV 2019.
- **[R4]** Yu, Liu, Odobez, "Deep multitask gaze estimation with a constrained landmark-gaze model," ECCV Workshops 2018.
- **[R5]** Ewaisha, El Shawarby, Abbas, Sobh, "End-to-end multitask learning for driver gaze and head pose estimation," Electronic Imaging 32:1–6, 2020.
- **[R6]** Wang, Chai, Venkatachalapathy, Tan, Haghighat, Velipasalar, Adu-Gyamfi, Sharma, "A survey on driver behavior analysis from in-vehicle cameras," IEEE T-ITS 23(8):10186–10209, 2021.
- **[R7]** "An efficient multi-task learning CNN for driver attention monitoring," ScienceDirect (Robotics and Autonomous Systems), 2024. https://www.sciencedirect.com/science/article/pii/S1383762124000225
- **[R8]** "Real-time driver fatigue detection system with deep learning on a low-cost embedded system" (Jetson Nano, dual CNN eye+mouth, YawDD, 96% acc.), ScienceDirect, 2023. https://www.sciencedirect.com/science/article/abs/pii/S0141933123000972
- **[R9]** Weng, Lai, Lai, "Driver drowsiness detection via a hierarchical temporal deep belief network," ACCV Workshops 2016.
- **[R10]** "DSDFormer: An Innovative Transformer-Mamba Framework for Robust High-Precision Driver Distraction Identification" (real-time on Jetson AGX Orin; SOTA on AUC-V1/V2, 100-Driver). https://arxiv.org/pdf/2409.05587
- **[R11]** "FedADAS: Communication-Efficient Federated Distillation for On-Device Driver Yawn Recognition in Vehicular Networks." https://arxiv.org/pdf/2605.19480
- **[R12]** "Human-Centered Benchmarking of Driver Monitoring Models" (MobileNetV3/ShuffleNetV2/EfficientNet-B0/DeiT-Tiny on MRL Eye Dataset). https://arxiv.org/pdf/2606.08123
- **[R13]** "Low-Cost Driver Monitoring System Using Deep Learning" (Raspberry Pi Zero 2W CNN). https://doaj.org/article/c901b04e8e5d4865a53b69f5335c701c
- **[R14]** "Real-Time Drivers' Drowsiness Detection and Analysis through Deep Learning," arXiv:2511.12438.
- **[R15]** "A Real-Time Driver Drowsiness Detection System Using MediaPipe and Eye Aspect Ratio," arXiv:2511.13618.
- **[R16]** "A Driver Gaze Estimation Method Based on Deep Learning," Sensors 22(10):3959, 2022. https://doi.org/10.3390/s22103959
- **[R17]** "Dual-Cameras-Based Driver's Eye Gaze Tracking System with Non-Linear Gaze Point Refinement," Sensors 22(6):2326, 2022 (PMC). https://pmc.ncbi.nlm.nih.gov/articles/PMC8949346/
- **[R18]** Naqvi, Arsalan, Batchuluun, Yoon, Park, "Deep Learning-Based Gaze Detection System for Automobile Drivers Using a NIR Camera Sensor," Sensors 18(2):456, 2018. https://www.ncbi.nlm.nih.gov/pmc/articles/PMC5855991/
- **Datasets:** DMD (Driver Monitoring Dataset), NTHU-DDD, YawDD, MRL Eye Dataset, AUC Distracted Driver V1/V2, 100-Driver, MPIIGaze, Gaze360.
- **Comms:** AUTOSAR SOME/IP Protocol Specification (AUTOSAR Standard 696); "The Ultimate Guide to AUTOSAR," https://www.acsiatech.com/the-ultimate-guide-to-autosar/ ; Elektrobit, "Automotive Ethernet Network," https://www.elektrobit.com/products/ecu/automotive-ethernet-automotive-networks/ ; LHP, "SOME/IP: Mastering the New Era of Vehicle Networks," https://www.lhpes.com/blog/some/ip-mastering-the-new-era-of-vehicle-networks ; Rumez et al., "Overview of Automotive SOAs," https://publikationen.bibliothek.kit.edu/1000130120/104022145
- **NVIDIA reference stack:** Jetson Linux GMSL camera framework docs, https://docs.nvidia.com/jetson/archives/r38.4/DeveloperGuide/SD/CameraDevelopment/JetsonVirtualChannelWithGmslCameraFramework.html ; Jetson Software Architecture (VPI/TensorRT/DeepStream), https://docs.nvidia.com/jetson/l4t/Tegra%20Linux%20Driver%20Package%20Development%20Guide/overview.html

## 0.6 Caveat
No Tier-1 or DMS specialist publishes exact production CNN backbones/hyperparameters. Company sections below state *only what that company has itself disclosed* (product pages, patents, interviews — cited), and separately label anything reconstructed from the general research literature as **"industry-inferred pattern"**.

---

# PART 1 — Company Deep Dives

## 1. Seeing Machines (FOVIO / e-DME / Occula NPU)

### 1.1 Detailed architecture — hardware, software, computer vision
Seeing Machines is a pure-play perception-IP licensor: it supplies the **algorithm stack** and, increasingly, dedicated **silicon IP**, to Tier-1s (Magna, Valeo, ADI) and OEMs (GM Super Cruise / Ford BlueCruise supervision cameras are built on this lineage).

**Hardware:**
- Camera: NIR (940 nm preferred — invisible to driver, less solar interference), mounted on **steering column or A-pillar** for driver-only DMS.
- Silicon evolution: **Xilinx/AMD Zynq-7000 SoC** (FPGA + ARM) for the original "FOVIO Chip" ASSP → Seeing Machines' own **Occula Neural Processing Unit (NPU)**, an application-specific accelerator described by the company as "highly optimized to accelerate algorithm architectures... found to be most effective for detecting and tracking humans," licensable as standalone silicon IP.
- 2023 platform refresh: partnership with **Analog Devices** (MAX25614 IR-LED driver + **GMSL SerDes**) and a camera-module partner, combining forward (8 MP) + cabin (5 MP) cameras on one open, multi-vendor SoC platform.

**Software (block diagram):**
```
 IR camera (A-pillar/column) --GMSL/CSI--> ISP --> e-DME perception engine
     |                                              |
     |                                              +- Face/eye detect + track
     |                                              +- Landmarking
     |                                              +- Head-pose + gaze regression (Occula-accelerated)
     |                                              +- Temporal drowsiness/distraction classifier
     |                                              +- Human-Factors rule layer (20+ yrs naturalistic-driving data)
     |                                              |
     +----------------------------------------------+--> CAN/Ethernet signal out (DMS/OMS/BdMS state)
```
- **e-DME (embedded Driver Monitoring Engine)** is explicitly designed to be **compute-agnostic** — it runs accelerated (Occula NPU) or unaccelerated (plain CPU/DSP) — giving Tier-1s a "low-friction integration pathway... across vehicle platforms and computer architectures."
- Three product lines from one core stack: **DMS** (driver-only), **OMS** (wide-FOV occupant sensing: seatbelt, child presence, phone-in-hand, gesture), **BdMS** (Backup-driver Monitoring System, for AV safety-driver supervision fleets).

### 1.2 End-to-end CV pipeline
- **Creation/data:** naturalistic-driving data collection programs (20+ years, per company messaging) feeding both the perception CNNs and the Human-Factors rule layer that converts raw perception into safety-relevant states.
- **Model families (industry-inferred pattern, consistent with company's own "neural inference" language and [R1]-[R5]):** lightweight CNN face/eye detectors + landmark regressors, feeding a gaze/head-pose regression head, feeding a temporal classifier for PERCLOS/microsleep and gaze-zone mapping — matching the shared industry pattern in Part 0. Seeing Machines' own material speaks specifically of "embedded neural inference processing" and "algorithm architectures... most effective for detecting and tracking humans," i.e., CNN-class detection/tracking pipelines, without publishing exact backbone names.
- **Training:** not publicly disclosed at the framework/hyperparameter level; company states its differentiation is in **optical-path co-design + Human-Factors science**, not just raw model accuracy — i.e., the training data collection/curation process (in-cabin, multi-demographic, multi-lighting) is treated as the core IP.
- **Deployment:** two deployment modes — Occula-NPU-accelerated (for the FOVIO Chip family) and plain-CPU/DSP fallback (e-DME), so the same trained models must be exportable/quantizable to both accelerated and non-accelerated targets — implying an ONNX-like intermediate representation and per-target quantization/compilation step.

### 1.3 Cameras, positions, execution architecture, communications
- Cameras: NIR, A-pillar or steering-column mount (DMS); wide-FOV variant for OMS.
- LiDAR: not used in the DMS pipeline (see Part 0.4).
- Comms: outputs standard DMS/OMS state signals to the vehicle network; because Seeing Machines licenses to many different Tier-1 E/E architectures, its output is designed to be transport-agnostic — packed as CAN/CAN-FD signals via Classic AUTOSAR COM/PduR, or exposed as a SOME/IP/DDS service on an Adaptive-AUTOSAR Ethernet backbone, depending on the integrating Tier-1's own architecture.

### 1.4 AI training pipeline & deployment (company-specific detail)
- Company describes its full stack as: **optical-path design -> algorithm development and validation "geared toward real-world driving environments" -> embedded software** — i.e., a co-design loop between camera/optics and the CNN training data distribution (glare, IR reflections off glasses, demographic variation) rather than a pure software-only ML pipeline.
- Deployment differentiator: FOVIO Chip family variants explicitly tuned for cost/performance tiers — a "basic Euro NCAP" variant vs. full-feature variants — implying a **model family with multiple compute/accuracy operating points** compiled to the same Occula NPU ISA, a pattern consistent with mobile/edge deployment practice generally (distillation to smaller student models per SKU).

### 1.5 Patents, papers, open-source mapping
- **Patents:** search assignee "Seeing Machines Limited" on Google Patents (patents.google.com) / Espacenet — large portfolio on gaze-tracking calibration, drowsiness state machines, optical design for IR imaging. No single patent number was independently verified in this research pass; treat company-specific patent claims as needing direct verification via the patent-office search tools above.
- **Company references:** https://seeingmachines.com/products/automotive/fovio-chip-family/ ; https://seeingmachines.com/products/automotive/ ; https://www.prnewswire.com/news-releases/seeing-machines-fovio-chip-optimized-for-ncap-readiness-300981572.html ; https://www.allaboutcircuits.com/news/seeing-machines-snags-numerous-partnerships-to-advance-adas/ ; https://ai-online.com/2021/08/seeing-machines/
- **General literature applicable to this architecture:** [R1]-[R6], [R16]-[R18].
- **Open-source analog for this pipeline stage:** `jhan15/driver_monitoring` (facial tracking + action-detection model trained on the **DMD** dataset) — https://github.com/jhan15/driver_monitoring

## 2. Smart Eye AB (incl. Affectiva Automotive AI, acquired 2021)

### 2.1 Detailed architecture — hardware, software, computer vision
Smart Eye is a high-fidelity eye-tracking specialist that merged with Affectiva (MIT-Media-Lab-lineage emotion-AI) to form what Smart Eye calls a "Human Insight AI" stack.

**Hardware:** single or **dual NIR camera** rigs, typically near the A-pillar/dashboard — dual cameras specifically to provide redundancy against single-camera occlusion (hand-on-face, extreme head yaw). Compute is camera/SoC-agnostic; Smart Eye's Affectiva Automotive AI SDK is licensed as software onto Tier-1-selected silicon (Qualcomm, NXP, TI, Ambarella).

**Software architecture:**
```
 NIR camera(s) (A-pillar/dash, single or dual) --> ISP --> Face/landmark CNN
                                                          |
                              +---------------------------+---------------------------+
                              |                                                       |
                    Head-pose regression                                    Appearance-based CNN
                                                                              gaze-vector regression
                              |                                                       |
                              +----------------> Gaze-zone fusion <--------------------+
                                                          |
                                    Online bias-correction / domain-adaptation layer
                                    (corrects offline-train vs. online-deploy shift)
                                                          |
                             Affectiva cognitive-state / expression classifiers (drowsiness,
                             distraction, emotion, cognitive load) — Action-Unit-based CNNs
                                                          |
                                              Vehicle-network signal output
```
- Key competitive claim (Smart Eye's own technical marketing): historically, automotive-grade gaze tracking used **model/geometry-based Pupil-Center-Corneal-Reflection (PCCR)**, which needs per-user calibration; Smart Eye layers **CNN-based appearance gaze regression** on top of/instead of PCCR specifically to remove that calibration step.
- A documented failure mode in this class of system — **"estimation bias between the training domain and the deployment domain"** causing predicted gaze points to drift from the true location — is explicitly why dual-camera, non-linear gaze-point-refinement methods exist in the surrounding research literature (Section 2.5).

### 2.2 End-to-end CV pipeline
- **Data creation:** Affectiva's pre-acquisition heritage is a large-scale, ethically-sourced, **human-annotated video dataset** engine (built originally for consumer emotion-AI, repurposed for cabin cognitive-state modeling) — Smart Eye's messaging credits Affectiva for "deep expertise in machine learning, data acquisition & annotation and AI ethics."
- **Model families:** (a) CNN-based face/landmark detection; (b) CNN appearance-based gaze-vector regression (the deep-learning alternative to PCCR); (c) Affectiva facial-action-unit (AU) CNNs for expression/cognitive-load classification; (d) temporal smoothing/refinement layer correcting online domain-shift bias.
- **Training:** large annotated video corpora (Affectiva heritage) + Smart Eye's own automotive eye-tracking datasets (the company's earlier commercial product, "Smart Eye Pro," is used as a benchmark ground-truth reference in third-party academic papers, e.g. [R16], which explicitly compares against "Smart Eye Pro 4.0").
- **Testing:** cross-subject validation is essential given appearance-based gaze CNNs are known to leak subject identity into gaze predictions if trained/tested on overlapping subjects — general best practice reflected in the literature the company's own claims are benchmarked against ([R16], [R17]).
- **Deployment:** software-only SDK (Affectiva Automotive AI) delivered to run on whatever SoC the Tier-1/OEM selects — implying a portable/quantization-friendly export path (ONNX or vendor SDK-specific, e.g. Qualcomm SNPE/QNN) rather than a fixed proprietary chip (contrast with Seeing Machines' Occula NPU strategy).

### 2.3 Cameras, positions, execution architecture, communications
- Cameras: single or dual NIR, A-pillar/dashboard mount.
- LiDAR: N/A for this DMS pipeline (Part 0.4).
- Comms: SDK output (gaze zone, drowsiness/distraction/emotion scores) is designed to be integrated into whatever Tier-1 comms stack hosts it — Classic AUTOSAR CAN signals for cluster warnings, or SOME/IP/DDS service exposure if hosted on an Adaptive-AUTOSAR cockpit domain controller (general industry pattern, Part 0.2).

### 2.4 AI training pipeline & deployment
- Training/eval loop is built around removing the calibration step that classical PCCR requires — meaning the CNN gaze-regression model must generalize zero-shot across new drivers without an enrollment step, a harder ML problem than person-specific gaze tracking, and the explicit reason Smart Eye markets its **deep-learning approach as an "innovative approach to driver gaze estimation"** versus legacy geometric methods.
- Deployment must handle the domain-shift problem identified in the third-party literature (dual-camera, non-linear refinement, [R17]) — i.e., production Smart Eye/Affectiva deployment pipelines are understood (from the surrounding research context this marketing draws on) to include an online refinement/calibration-free correction stage after the core CNN inference, not just a single forward pass.

### 2.5 Patents, papers, open-source mapping
- **Company references:** https://go.smarteye.se/combating-driver-distraction-with-innovative-approach-to-driver-gaze-estimation
- **Directly relevant third-party research (benchmarks against Smart Eye's commercial tracker or the same problem class):**
  - "A Driver Gaze Estimation Method Based on Deep Learning," Sensors 22(10):3959, 2022 (benchmarks against Smart Eye Pro 4.0) — https://doi.org/10.3390/s22103959
  - "Dual-Cameras-Based Driver's Eye Gaze Tracking System with Non-Linear Gaze Point Refinement," Sensors 22(6):2326, 2022 — https://pmc.ncbi.nlm.nih.gov/articles/PMC8949346/
  - Kellnhofer et al., Gaze360, ICCV 2019; Zhang et al., MPIIGaze, IEEE TPAMI 2017 (foundational appearance-based gaze CNN datasets this class of product is built on).
- **Patents:** search assignee "Smart Eye AB" and "Affectiva Inc" on Google Patents/Espacenet for gaze-calibration-free regression and facial-action-unit classification IP.
- **Open-source analog:** MediaPipe Face Mesh + EAR/PERCLOS pipelines (e.g., `erlendskinnemoen/Driver-Monitoring-Systems-using-Google-s-MediaPipe-FaceMesh` — https://github.com/erlendskinnemoen/Driver-Monitoring-Systems-using-Google-s-MediaPipe-FaceMesh) approximate the landmark/EAR stage; no open-source equivalent exists for Affectiva's proprietary AU/emotion CNNs.

## 3. Bosch (Driver Monitoring Camera / Occupant Monitoring Camera + Cabin Sensing Radar)

### 3.1 Detailed architecture — hardware, software, computer vision
Bosch is a **full-stack Tier-1**: it supplies camera hardware, radar, ECU/domain-computer integration, and the AI models end-to-end (unlike Seeing Machines/Smart Eye, which are software/IP licensors).

**Hardware:**
- **Driver Monitoring Camera (DMC)** — narrow FOV, driver-only, mounted on steering column, A-pillar, or dashboard.
- **Occupant Monitoring Camera (OMC)** — wide FOV, mounted above/below the rear-view mirror or in the central display, covering driver + front passenger + partial rear seats; detects phone-in-hand, unfastened rear seatbelt, forgotten objects/children.
- **Cabin Sensing Radar** — short-range radar in the headliner/overhead console, detecting sub-millimeter chest-motion/vital signs; explicitly built to catch failure modes camera-only systems miss (e.g., a sleeping infant under a blanket in a rear-facing car seat, or a child in the footwell) — fused at feature/decision level with the camera outputs.
- **Integration flexibility (explicitly documented by Bosch):** DMS/OMS logic can run (a) in a dedicated interior-sensing ECU, (b) inside a cockpit/cross-domain computer, or (c) as a software-only stack on an existing information/domain computer — Bosch frames this as adapting to whatever E/E architecture (federated vs. zonal/domain) the OEM has chosen.

**Software / block diagram:**
```
 DMC (steering col/A-pillar/dash)     OMC (mirror/center display)      Cabin radar (headliner)
        |                                     |                                |
        +----------------+--------------------+----------------+---------------+
                         |                                     |
                 Gaze direction, eye-opening,          Object/occupant classification
                 posture feature extraction                (phone, bag, child, seatbelt)
                         |                                     |
                         +------------------+------------------+
                                            |
                          AI fusion layer: "draws correct conclusions" ->
                          drowsiness/distraction state, break recommendation,
                          or (automated driving) MRM trigger
                                            |
             Interior-sensing ECU  OR  cockpit/cross-domain computer  OR  info-domain-computer SW stack
                                            |
                                CAN / Ethernet vehicle-signal output
```

### 3.2 End-to-end CV pipeline
- **Creation/data:** Bosch's own engineering blog states the team (AI interior sensing group, Bosch Cross-Domain Computing Solutions division) uses **synthetic data generation / rendered cabin scenes** specifically to avoid slow, costly real-world recording campaigns and to speed development across *multiple camera types and mounting positions* across different vehicle models — an explicit synthetic-to-real domain-adaptation training strategy, named by team members (Jochen Kall, Fabian Märkert, Alen Smajic, Bernd Göbelsmann, Dennis Mack, Paul Robert Herzog).
- **Model families (from Bosch's own feature description):** the DMC/OMC use "artificial intelligence" to interpret gaze direction, eye-opening, and posture; the OMC additionally performs object detection (phone, bag) — implying at minimum a face/landmark CNN + object-detection head (YOLO/SSD-class), consistent with the shared industry pattern.
- **Training:** synthetic-data-first pipeline (rendered cabin/occupant scenes) supplemented with real-world validation data, feeding CNN detection/classification models; radar feature fusion trained/tuned separately (radar signal processing + classical or learned vital-sign extraction) then fused with camera outputs.
- **Testing:** must satisfy Euro NCAP and EU GSR compliance testing explicitly referenced in Bosch's own engineering-services page ("in full compliance with the EU General Safety Regulation (GSR) and NCAP consumer tests until at least 2026").
- **Deployment:** three deployment targets (dedicated ECU / cross-domain cockpit computer / software-only on info-domain computer) — implying the trained models must be portable across at least three different compute environments, again implying an ONNX/vendor-SDK-portable export pipeline plus per-target quantization.

### 3.3 Cameras, positions, execution architecture, communications
- DMC: steering column / A-pillar / dashboard.
- OMC: above/below rear-view mirror, or center display.
- Cabin radar: headliner / overhead console.
- LiDAR: not part of the interior-sensing sensor suite (Part 0.4) — Bosch's radar performs the "see-through-occlusion" role LiDAR might otherwise play, but at much lower cost/power and without the eye-safety/interior-space constraints LiDAR would impose.
- Comms/E-E architecture: Bosch explicitly documents that, depending on OEM E/E architecture, the interior-sensing function is wired via **a dedicated ECU on CAN/CAN-FD (Classic AUTOSAR COM/PduR stack)**, or **integrated into a central vehicle/cockpit computer** reachable via **Automotive Ethernet with SOME/IP or DDS service exposure (Adaptive AUTOSAR)** — Bosch is one of the AUTOSAR consortium's founding members, so both Classic and Adaptive AUTOSAR toolchains are natively supported in its own ECU software stack.

### 3.4 AI training pipeline & deployment (company-specific detail)
- Explicit synthetic-data pipeline reduces "time-consuming recording campaigns," and Bosch states this "also applies to other camera types with different installation positions in a variety of vehicle types" — i.e., a **parameterized rendering pipeline** (camera intrinsics/extrinsics, cabin geometry, lighting, occupant pose/demographics as simulation parameters) generating labeled synthetic training data at scale, then presumably fine-tuned/validated on real fleet data before production release.
- Deployment output feeds directly into an **escalation ladder**: warning icon/chime → break recommendation → (for automated-driving-capable vehicles) a **safe stop / minimal risk maneuver (MRM)**, explicitly gated by "the vehicle manufacturer's wishes and applicable legal regulations" — i.e., the decision thresholds are OEM-configurable on top of a shared perception stack.

### 3.5 Patents, papers, open-source mapping
- **Company references:** https://www.bosch-mobility-solutions.com/en/solutions/interior/interior-monitoring-systems/ ; https://www.bosch-mobility.com/en/solutions/interior/interior-sensing-cv/ ; https://www.bosch-engineering.com/services/mobility-solutions/adas/interior-sensing/ ; https://www.bosch.com/stories/ai-interior-sensing/ ; https://us.bosch-press.com/pressportal/us/en/press-release-9984.html
- **Patents:** search assignee "Robert Bosch GmbH" + "driver monitoring" / "occupant monitoring" / "cabin sensing radar" on Google Patents/Espacenet — Bosch's automotive-sensing patent portfolio is very large; no single patent number independently verified in this pass.
- **General literature applicable:** [R1], [R6], [R7] (multi-task attention CNNs), [R9] (temporal drowsiness modeling — analogous to Bosch's AI fusion layer).
- **Open-source analog:** synthetic-data-driven CV training is broadly analogous to open frameworks like NVIDIA Omniverse Replicator or Blender-based synthetic-data pipelines used in academic driver-monitoring work; no Bosch-specific open-source release exists (proprietary).

## 4. Valeo (Driver & Interior Monitoring System / "Cocoon")

### 4.1 Detailed architecture — hardware, software, computer vision
Valeo is a Tier-1 with its own dome-module hardware and deep-learning DMS "in mass production," which in 2024 restructured its perception-software relationship with Seeing Machines (see below).

**Hardware:**
- Dashboard-mounted camera for driver-only DMS; wide-FOV 2D RGB-IR / RGB / RGB-Low-Light dome-module camera (2.5–5 Mpx) for the broader Interior Monitoring System ("Cocoon").
- A dedicated compact **3D camera** inside the dome module supports gesture recognition as a separate ML task on the same hardware platform.
- Scalable ECU: integrable as a standalone interior-sensing controller, or into Valeo's **Smart Front Camera-centric architecture ("Smart Safety 360")**, where the forward ADAS camera acts as the vehicle's central low-cost computer for L2/L2+ programs — reflecting the industry's camera-as-central-computer / satellite-camera trend.

**Software / block diagram:**
```
 Dashboard DMS camera         Dome-module wide-FOV RGB-IR camera + 3D camera
        |                                |
   Deep-learning DMS stack:      Deep-learning IMS stack:
   - Face ID / driver ID         - Age/gender/clothing classification
   - Head-and-eye tracking       - Posture/occupant classification (-> airbag timing/intensity)
   - Gaze-based distraction/     - Gesture recognition (3D camera ML)
     drowsiness scoring          - Life-detection (child-left-behind, EuroNCAP roadmap)
        |                                |
        +----------------+---------------+
                         |
           Valeo scalable ECU / Smart Front Camera domain computer
                         |
             CAN / Ethernet vehicle-signal output
```
- 2024 restructuring: Valeo **transferred its own DMS/OMS perception-software activity** (including its acquisition of German AI-perception startup **Asaphus**) to **Seeing Machines**, while Valeo continues to integrate Seeing Machines' perception software into Valeo's own hardware/ECU/system architecture. This is the clearest publicly documented example of the industry's **Tier-1 hardware/systems-integration + specialist third-party perception-IP** pairing pattern (mirrored, e.g., by many Tier-1s licensing Smart Eye/Affectiva or Cipia software onto their own hardware).

### 4.2 End-to-end CV pipeline
- **Model families (per Valeo's own product description):** deep-learning face ID/recognition, head-and-eye tracking (gaze), age/gender/clothing-level classification, posture classification, gesture recognition (from the dedicated 3D camera), and life-detection — spanning face-recognition CNNs, landmark/gaze-regression CNNs, and body-pose/gesture classifiers, consistent with the shared architecture pattern (Part 0.2 items c/d/g/h).
- **Data/training:** not independently disclosed by Valeo beyond "deep learning algorithms... in mass production"; the Asaphus acquisition (a German AI-perception startup) is explicitly cited as bringing "unique intellectual property, data and a specialized development team skilled in machine learning and AI for driving and sensing features" — i.e., Valeo's (pre-2024) training data/IP pipeline was built substantially around this acquired team, now transferred to Seeing Machines.
- **Testing:** DMS explicitly contributes "25% of the ADAS evaluation and occupant condition monitoring to EuroNCAP," per Valeo's own product page — i.e., validation is directly tied to the EuroNCAP DMS test protocol.
- **Deployment:** scalable ECU designed to integrate into either a dedicated interior-sensing architecture or the Smart Front Camera central-computer architecture — again implying a portable model-export pipeline across at least two different compute targets.

### 4.3 Cameras, positions, execution architecture, communications
- DMS camera: dashboard-mounted.
- IMS dome-module camera: wide-FOV RGB-IR/RGB/RGB-Low-Light + 3D camera, roof/dome-mounted (mirror area).
- LiDAR: Valeo does build automotive LiDAR (e.g., on the Mercedes S-Class/Honda Legend L3 systems, per Valeo's own ADAS page), but this is used for **exterior** perception, not the interior DMS/IMS pipeline (Part 0.4).
- Comms: Valeo's DMS/IMS is explicitly described as "integrable in Valeo Electronic Control Unit (ECU) or in Domain Controller" — i.e., either a Classic-AUTOSAR CAN-connected standalone ECU, or a service exposed from a domain controller over Automotive Ethernet (SOME/IP/DDS under Adaptive AUTOSAR), matching the general pattern in Part 0.2/Section on comms below.

### 4.4 AI training pipeline & deployment
- Company material emphasizes "deep learning algorithms, including a scalable ECU" — the scalability claim implies the same trained model family is deployed at multiple compute/feature tiers (basic drowsiness/distraction only, vs. full DMS+IMS+gesture+life-detection), a common industry deployment pattern of a shared backbone with feature-specific heads enabled/disabled per SKU.
- Post-2024, Valeo's system-level deployment pipeline consumes **Seeing Machines' e-DME perception software** (Section 1) as the algorithmic core, integrated into Valeo's own ECU/domain-controller hardware and validation/testing pipeline — i.e., Valeo's "AI training pipeline" for DMS specifically is now largely inherited from Seeing Machines' pipeline (Section 1.4), while Valeo's own training/data pipeline continues independently for the IMS-specific tasks (posture, gesture, life-detection) it retained.

### 4.5 Patents, papers, open-source mapping
- **Company references:** https://www.valeo.com/en/interior-cocoon/ ; https://www.valeo.com/en/catalogue/cda/in-vehicle-monitoring-system/ ; https://www.valeo.com/en/catalogue/cda/driver-monitoring-system/ ; https://www.valeo.com/en/catalogue/cda/smart-front-camera/ ; https://www.valeo.com/en/catalogue/cda/valeo-smart-safety-360/ ; https://www.autonomousvehicleinternational.com/news/sensors/valeo-and-seeing-machines-partner-on-driver-and-occupant-monitoring-systems.html
- **Patents:** search assignee "Valeo" + "occupant monitoring" / "driver monitoring" and "Asaphus" on Google Patents/Espacenet.
- **General literature applicable:** [R1] (multi-task face/pose/gaze CNN, directly analogous to Valeo's combined face-ID+gaze+posture stack), [R6] (driver-behavior-from-camera survey).
- **Open-source analog:** no Valeo-specific open-source release; MediaPipe-based posture/gesture pipelines and `jhan15/driver_monitoring`'s DMD-trained action-detection model are the closest open-source functional analogs for the IMS posture/gesture tasks.

## 5. Cipia (formerly Eyesight Technologies) — Driver Sense / Cabin Sense

### 5.1 Detailed architecture — hardware, software, computer vision
Cipia is a **software-first** computer-vision company; its DMS ("Driver Sense") and OMS ("Cabin Sense") products are licensed hardware-agnostically to OEMs/Tier-1s, with an optional standalone reference-hardware unit for the aftermarket fleet/telematics channel.

**Two-layer architecture (explicitly described by Cipia's own product team):**
```
 Camera (OEM-selected hardware, or Cipia reference dongle)
        |
 LAYER 1 - PERCEPTION (computer vision / ML)
   Face detection -> landmarking -> eye-state / gaze direction -> object-in-hand
   classification (phone, cigarette)
        |
 LAYER 2 - HUMAN-FACTORS INTERPRETATION (translates CV outputs into states)
   Drowsy / distracted / driver-identity-match / specific "driver actions"
   classification, using human-factors-science-based rules on top of Layer-1 output
        |
 Deployment target A: embedded into OEM production ECU/SoC
 Deployment target B: standalone hardware unit (camera + embedded compute) for
                       fleet/telematics channel
```
- This explicit **perception vs. cognitive-state-inference** separation mirrors the architecture used by Seeing Machines (e-DME + Human-Factors layer) and Smart Eye (CNN gaze + Affectiva cognitive-state layer) — i.e., it is the dominant industry pattern, not unique to Cipia, but Cipia is the company that has most explicitly named the two layers in public interviews.
- Predictive extension (per associated patent, Section 5.5): a **deep recurrent LSTM network** anticipates driver behavior/action a few seconds *before* it happens, using multi-modal sensor fusion (video + tactile sensors + GPS) — i.e., Cipia's architecture (or a closely related in-cabin-sensing patent family in this space) is explicitly predictive, not purely reactive.

### 5.2 End-to-end CV pipeline
- **Model families (patent-documented):** the patent literature in this space explicitly lists the candidate model classes considered: "linear and logistic regression, linear discriminant analysis, support vector machines (SVM), decision trees, random forests, ferns, Bayesian networks, boosting, genetic algorithms, simulated annealing, or **convolutional neural networks (CNN)**" for the perception layer, and "multi-layered perceptrons, convolutional neural networks, deep neural networks, deep belief networks, autoencoders, **long short-term memory (LSTM) networks**, generative adversarial networks, and deep reinforcement networks" for the deep-learning layer — with the LSTM explicitly called out as the mechanism for **anticipatory behavior prediction**.
- **Training:** the same patent family describes **online/in-field learning** — the object-in-hand classifier is described as initially failing to identify a novel held object (e.g., "unable to identify that the object is a cellphone"), then being retrained via AI learning techniques to correctly classify it and subsequently detect related behaviors (e.g., texting) — i.e., a continual-learning / field-feedback retraining loop is part of the documented architecture, not just a one-time offline training pass.
- **Testing:** the same patent describes the system learning to determine a **distraction-severity threshold conditioned on vehicle operating conditions** at the time of the behavior — i.e., contextual, condition-dependent decision thresholds validated against real driving scenarios rather than a single fixed global threshold.
- **Deployment:** dual deployment path (embedded OEM SoC vs. standalone hardware unit) implies, as with the other vendors above, a portable model-export pipeline plus per-target quantization/compilation.

### 5.3 Cameras, positions, execution architecture, communications
- Camera: OEM/Tier-1-selected hardware (software-only licensing model) for production integration; Cipia also ships its own reference hardware (camera + embedded compute) for the fleet/telematics channel.
- LiDAR: N/A (Part 0.4).
- Comms: for OEM production integration, Cipia's software runs inside the OEM's own ECU/SoC and communicates via whatever comms stack that ECU exposes (Classic AUTOSAR CAN or Adaptive AUTOSAR SOME/IP/DDS, per the OEM's own E/E architecture — general pattern, Part 0.2); for the standalone fleet-device channel, the device instead typically reports over cellular/telematics rather than the vehicle's internal CAN/Ethernet bus.

### 5.4 AI training pipeline & deployment
- Documented (via patent) as a **continual/field-adaptive pipeline**: initial offline-trained perception + interpretation models are deployed, then refined in the field as novel objects/behaviors are encountered, with the distraction-severity threshold itself being learned/adjusted per vehicle operating condition — i.e., Cipia's (or this patent family's) deployment pipeline explicitly includes an **online model-update loop**, not just a static, frozen production model.

### 5.5 Patents, papers, open-source mapping
- **US20210269045A1**, "Contextual driver monitoring system" — deep recurrent LSTM anticipatory behavior modeling, multi-sensor (video/tactile/GPS) fusion. https://patents.google.com/patent/US20210269045A1/en
- **US Patent 12,403,931** (issued Sept 2, 2025), "Vehicular driver monitoring system" — AI-trained object-in-hand (cellphone) recognition with online/adaptive distraction-threshold learning conditioned on vehicle operating state. https://patents.justia.com/patent/12403931
- **Company references:** https://www.just-auto.com/interview/beyond-driver-monitoring-qa-with-cipia/ (Cipia's own description of the two-layer perception + human-factors architecture); https://idtechwire.com/cipia-unveils-new-driver-monitoring-system-022501/ (Driver Sense launch, gaze tracking + blink-rate + smoking/phone detection); https://www.marklines.com/en/news/289395 (OEM production integration)
- **General literature applicable:** [R9] (temporal/hierarchical drowsiness modeling, analogous to the LSTM anticipatory layer).
- **Open-source analog for the perception layer:** any of the MediaPipe/EAR-based repos in Part 0's dataset/open-source appendix approximate Layer-1; no open-source equivalent exists for the patented LSTM anticipatory-behavior layer.

## 6. Continental (Cockpit HPC + combined front/interior camera)

### 6.1 Detailed architecture — hardware, software, computer vision
Continental integrates DMS as part of a broader **cockpit domain / High-Performance Computer (HPC)** strategy rather than as an isolated ECU, and is one of the few companies to publicly describe **combining the forward ADAS camera and the interior driver camera into a single system**.

**Hardware:**
- **"Everything in View" combined camera system**: an interior camera (detecting driver position, gaze direction, hand location) paired with the forward road camera, explicitly positioned as **a prerequisite for automated-driving mode handover** — the system continuously determines whether the driver is able to take over responsibility for driving.
- **Smart Cockpit High-Performance Computer (HPC)**: integrates cluster + infotainment + ADAS functions (including DMS) into one box, supporting **up to five cameras** in typical configurations, built on **Telechips' "Dolphin" SoC family**.
- **Biometric-sensing display** (CES 2025 demo): 3D distance mapping for airbag deployment and seatbelt monitoring, contactless biometric sensing for heart-rate tracking, touch-based skin-moisture sensing to estimate blood-alcohol content, and object-in-hand recognition (demoed with a child's toy) — i.e., Continental's cockpit-sensing roadmap extends DMS into biometric/health sensing.
- **Ambarella CV3-AD partnership** (for the broader ADAS/AD domain-controller SoC, into which DMS increasingly consolidates): Ambarella's CV3-AD family performs centralized, single-chip processing for multi-sensor perception — **camera, radar, ultrasonic, and LiDAR** fusion — for L2+ through higher automation levels; Continental contributes hardware + most of the software, Ambarella the SoC + additional software.

**Software / block diagram:**
```
 Interior camera (combined module with forward camera)      Forward ADAS camera
                |                                                    |
        Driver position, gaze direction,                    Lane/object/traffic detection
        hand-location detection                                     |
                +----------------------------+-----------------------+
                                             |
                        Handover-readiness fusion logic:
                        "is the driver able to take responsibility for driving?"
                                             |
                Smart Cockpit HPC (Telechips Dolphin SoC) <-or-> Ambarella CV3-AD domain controller (broader AD stack)
                                             |
                          CAN / Automotive-Ethernet vehicle-signal output
```

### 6.2 End-to-end CV pipeline
- **Model families:** not independently disclosed at the CNN/backbone level by Continental; the explicit feature set (gaze direction "with certainty," hand-location detection, 3D distance mapping) implies at minimum a landmark/gaze-regression CNN plus a 3D/depth-based body-pose model for the biometric-sensing display, consistent with the shared industry pattern (Part 0.2).
- **Deployment tiering:** two distinct deployment contexts are documented — (a) a **cockpit-domain HPC** (Telechips Dolphin) handling cluster/infotainment/DMS together, and (b) a separate, higher-compute **AD domain controller** (Ambarella CV3-AD) handling full sensor-fusion perception for higher automation levels — implying DMS models must be portable/scalable across at least these two very different compute tiers.
- **Testing/validation:** explicitly framed around the automated-driving handover use case — i.e., validation must demonstrate that the fused interior+exterior signal reliably predicts whether a human can safely resume control, a functional-safety-relevant (ISO 26262/SOTIF) test requirement beyond simple drowsiness/distraction accuracy.

### 6.3 Cameras, positions, execution architecture, communications
- Interior camera: integrated into the same module as the forward-facing ADAS camera (behind windshield / near-mirror area), per Continental's "Everything in View" architecture — also separately, Continental supplies a driver camera **integrated into the digital instrument cluster** for BMW iX, per Continental's Cockpit HPC OEM win.
- LiDAR: present in Continental's broader AD sensor suite (via the Ambarella CV3-AD fusion chip) but for **exterior** perception, not the interior DMS camera pipeline (Part 0.4).
- Comms: cockpit HPC integrates cluster+infotainment+ADAS into a single box specifically to **reduce the number of previously installed control units and wiring harnesses**, implying a shift from federated CAN-per-function wiring toward a **centralized Ethernet backbone with SOME/IP/DDS service exposure** internally, while still supporting legacy CAN/CAN-FD for body/chassis signal integration (general Adaptive-AUTOSAR zonal-architecture pattern, Part 0.2/comms section).

### 6.4 AI training pipeline & deployment
- Not independently disclosed at the training-pipeline level; the multi-tier deployment (cockpit HPC vs. AD domain controller) strongly implies a shared perception-model family with per-tier compute/accuracy trade-offs, and the generative-AI dialogue-system partnership with **Google Cloud** (integrated into the same Smart Cockpit HPC) indicates Continental's cockpit-domain software stack is increasingly a **mixed classical-CV/DNN + cloud-connected generative-AI** platform, of which DMS is one function among several sharing the same HPC compute budget.

### 6.5 Patents, papers, open-source mapping
- **Company references:** https://www.continental.com/en/press/press-releases/road-and-driver-camera/ (combined front+interior camera architecture) ; https://www.continental.com/en/press/press-releases/20231214-telechips/ and https://www.continental.com/en/press/press-releases/20230620-smart-cockpit/ (Smart Cockpit HPC, Telechips Dolphin SoC) ; https://www.ambarella.com/news/continental-and-ambarella-partner-on-assisted-and-automated-driving-systems-with-full-software-stack/ (CV3-AD partnership, camera+radar+ultrasonic+LiDAR fusion) ; https://www.techbrew.com/stories/2025/01/30/auto-supplier-continental-software-defined-vehicle-tech (CES 2025 biometric-sensing cockpit demo)
- **Patents:** search assignee "Continental Automotive" / "Continental AG" + "driver monitoring" / "occupant sensing" on Google Patents/Espacenet.
- **General literature applicable:** [R1], [R6], [R7] (multi-task CV covering gaze + pose + object-in-hand, matching Continental's documented feature set).
- **Open-source analog:** no Continental-specific open-source release; the combined-camera handover-readiness concept is functionally analogous to academic driver-state-plus-road-context fusion work (e.g., the same gaze-zone + road-context correlation idea Mobileye later productized, Section 9).

## 7. DENSO (Driver Status Monitor, DSM/DN-DSM) + Xperi/FotoNation partnership

### 7.1 Detailed architecture — hardware, software, computer vision
DENSO is one of the longest-running DMS suppliers (first product **2014**, for heavy trucks/buses), and is unusually transparent about its optics and data-pipeline design.

**Hardware (company-documented in detail):**
- **Camera**: CMOS image sensor emitting **near-infrared rays** (invisible to the human eye) for consistent brightness regardless of day/night cabin lighting.
- **Proprietary lens**: DENSO developed a **lens integrating a cylindrical lens with a prism** specifically to illuminate the driver's face with controlled NIR lighting from the LED — a documented optical-engineering innovation distinct from a simple IR-LED-ring approach.
- **Optical filter**: reduces visible light specifically to **facilitate eye recognition when the driver wears sunglasses**.
- **Industrial design**: the camera housing (DN-DSM) uses a faceted, multi-angled lens surface deliberately so the device does **not look like a "surveillance camera"**, reducing the driver's psychological discomfort at being watched — an explicit human-factors/industrial-design decision documented by DENSO's design team (winner, Good Design Award).
- **Mount position**: **above the steering column**, on production Toyota/Lexus vehicles (e.g., 2016 Lexus LS460 "Driver Monitor System"/"Driver Attention Monitor," part of the Advanced Pre-Collision System Package alongside Lane Keep Assist and Dynamic Radar Cruise Control).

**Software (block diagram):**
```
 NIR camera (steering-column mount, cylindrical-lens+prism optics, IR optical filter)
        |
 DENSO's proprietary Recognition Algorithm:
   - Facial-feature identification: eyes, nose, mouth, facial contours
   - Eyelid-movement (blink/closure) detection
   - Face-angle / head-position detection
        |
 (2017+) FotoNation CNN-based upgrade layer:
   - Face detection, head pose, eye gaze, occlusion analytics (deep-learning based)
        |
 Drowsiness / distraction / inappropriate-posture classification
        |
 Warning: display message + audible chime (production Toyota/Lexus integration)
        |
 (commercial-vehicle variant) SD-card logging + telematics device + cloud digital
 tachograph (Fujitsu) integration for fleet operation-manager review
```
- **2017 partnership with Xperi/FotoNation**: DENSO explicitly announced joint development to bring **"facial image recognition and neural network technologies"** into the next-generation DSM, specifically to move beyond the earlier "relative positions of eyes/nose/mouth" heuristic approach toward **CNN-based** detection with more features, plus new capabilities (more accurate gaze direction, facial-expression/state-of-mind estimation).
- **2019 Shanghai Motor Show showcase**: the joint DENSO+FotoNation DMS explicitly demonstrated **"FotoNation's most advanced convolutional neural network (CNN) based algorithms"** performing **face detection, head pose, eye gaze, and occlusion analytics** — this is one of the few instances in this research where a company (Xperi/FotoNation) explicitly and publicly confirms "CNN" as the model family in a joint OEM demonstration.

### 7.2 End-to-end CV pipeline
- **Data creation:** DENSO explicitly states its facial-recognition product was trained/validated using **"a facial image database of more than 6,000 people,"** specifically because eye characteristics differ significantly person-to-person — one of the very few instances in this research where a company discloses an approximate training/validation dataset size.
- **Model families:** early generations (2014+) used DENSO's own classical/heuristic "Recognition Algorithm" (facial-feature-position based); 2017+ generations incorporate **FotoNation's CNN-based deep-learning technology** for face detection/head-pose/eye-gaze/occlusion analytics — a clear, company-confirmed **transition from classical CV heuristics to CNN-based deep learning** across DENSO's own product generations.
- **Training/continual improvement (patent-documented):** DENSO's patent **US20230252759A1** describes a system that generates a **training dataset specifically to give corrective feedback to a neural network for object detection** — i.e., an active-learning/feedback loop where the network's errors ("you got this wrong, here's the correct answer") are used to iteratively retrain/improve the object-detection model in the field, extending the DSM beyond face/eye states to more general object-in-cabin recognition.
- **Testing:** DENSO's product covers distraction, drowsiness, sleep, and inappropriate posture — implying a multi-class classification test suite spanning at least these four driver-state categories, validated against DENSO's own facial-image database plus real commercial-fleet deployment data (given its long production history with trucks/buses since 2014).
- **Deployment:** two very different deployment contexts are documented — (a) OEM-integrated passenger-car deployment (Toyota/Lexus, steering-column camera feeding the vehicle's own warning display/chime) and (b) a **retrofittable standalone commercial-vehicle unit** (camera + onboard processing + SD-card logging + telematics/cloud integration with a Fujitsu digital tachograph) — the latter is a self-contained edge deployment with local storage and fleet-cloud connectivity, distinct from a pure in-vehicle-network-integrated deployment.

### 7.3 Cameras, positions, execution architecture, communications
- Camera: steering-column mount (above the wheel), NIR with cylindrical-lens+prism optics and an IR-focused optical filter to work around sunglasses.
- LiDAR: N/A for DSM (Part 0.4).
- Comms: OEM passenger-car integration reports into the vehicle's existing cluster/warning system — historically via Classic AUTOSAR CAN signals feeding the instrument cluster (DENSO is a major Classic-AUTOSAR ECU supplier generally); the retrofittable commercial-vehicle unit instead reports via **onboard SD-card storage plus a telematics device**, syncing to a cloud-based digital tachograph (Fujitsu) rather than the vehicle's internal CAN/Ethernet bus — i.e., DENSO documents two entirely different "communication architectures" for its two product lines (in-vehicle-network-integrated vs. telematics/cloud-based).

### 7.4 AI training pipeline & deployment
- Company-confirmed generational shift: **heuristic facial-feature-position algorithm (2014) → FotoNation CNN-based deep learning (2017 partnership, 2019 showcase)** — this is one of the clearest, most explicit company-confirmed "before/after" AI-architecture transitions found in this research pass.
- Patent US20230252759A1 documents an explicit **feedback-driven continual training loop** for the object-detection neural network component — i.e., DENSO's deployment pipeline is documented (via patent) to include field-error correction rather than being a purely static, frozen model.
- Fleet variant's deployment pipeline explicitly closes the loop back to a human "operation manager," who reviews logged driver-condition data/images per alert — i.e., part of DENSO's "deployment" is a human-in-the-loop fleet-management review process, not purely automated in-vehicle response.

### 7.5 Patents, papers, open-source mapping
- **US20230252759A1** (Denso International America) — neural-network object-detection training-feedback system for the Driver Status Monitor. Discussed in: https://whatafuture.com/denso-smarter-guidebook-driver-monitor
- **US Patent 6,927,694** (earlier-generation, pre-CNN heuristic) — "Algorithm for monitoring head/eye motion for driver alertness with one camera" (head-nod frequency, blink/closure frequency, yawn frequency thresholding) — https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6927694 — illustrates the classical-CV baseline DENSO's later CNN-based system superseded.
- **Company references:** https://www.denso.com/global/en/business/products-and-services/mobility/pick-up/dsm/ (6,000-person facial database, optics detail) ; https://www.automotiveworld.com/news-releases/denso-fotonation-collaborate-image-recognition-technology/ and https://www.densomedia-na.com/news/denso-fotonation-collaborate-image-recognition-technology/ (2017 FotoNation partnership announcement) ; https://investor.xperi.com/news/news-details/2019/DENSO-Driver-Monitoring-System-Featuring-FotoNation-Technology-to-be-Showcased-at-the-Shanghai-Motor-Show/default.aspx (2019 CNN showcase — explicit "convolutional neural network" confirmation) ; https://www.prnewswire.com/news-releases/denso-releases-new-safety-monitor-to-reduce-accidents-involving-commercial-vehicles-300648665.html (retrofittable commercial-vehicle unit, SD-card + telematics + Fujitsu digital-tachograph integration) ; https://design.denso.com/en/works/2018/10/dn-dsm.html (industrial-design/optics detail, Good Design Award)
- **General literature applicable:** [R1] (multi-task face detect+landmark+pose, matching FotoNation's documented face-detection/head-pose/eye-gaze/occlusion feature set), [R18] (NIR-camera deep-learning gaze detection, directly analogous to DENSO+FotoNation's NIR+CNN pipeline).
- **Open-source analog:** none specific to DENSO/FotoNation (proprietary); NIR-camera CNN gaze pipelines in the open literature ([R18]) are the closest functional/methodological analog.

## 8. Magna (incl. Veoneer interior-sensing heritage) — Mirror-Integrated DMS/OMS

### 8.1 Detailed architecture — hardware, software, computer vision
Magna's defining architectural decision, per its own Product Manager for Interior Sensing (Tom Herbert) and Director of Innovation and Research (Tobias Aderum), is **camera integration inside the frameless interior (rear-view) mirror** — described by Magna as a breakthrough because "no one thought the DMS could be mounted on the mirror, because it moves," requiring the camera/algorithm to actively compensate for mirror adjustment.

**Hardware:**
- Camera integrated into the **frameless interior mirror module**, chosen because the mirror provides "the best vantage point" in the cabin — an unobstructed view of the driver and, with a wider lens, the rear-seat passengers too.
- Additional camera application points: **center stack** and **steering wheel**, primarily driver-focused, supplementing the mirror-mounted primary sensor — reflecting Magna's Group-wide "interoperability" across its mirror, electronics, and interior-sensing business units (a stated unique selling point versus competitors who do not have an in-house mirror business).
- Scalable feature set on the same hardware: **child-presence detection, seatbelt detection**, and even **video-conferencing and facial-recognition** convenience features share the same camera/compute platform as core drowsiness/distraction DMS.

**Software / block diagram:**
```
 Mirror-integrated camera (compensates for mirror repositioning)
        |
 Core DMS/OMS perception: face detect+track, gaze/head-pose, drowsiness/distraction
        |
 Feature-scalable outputs (same platform): child-presence detection, seatbelt
 detection, facial recognition/ID, video-conferencing framing
        |
 "Collective Perception" concept: DMS output combined with V2X/vehicle-to-vehicle
 context sharing (per Aderum: "When vehicles are talking to each other, our
 cameras solve a lot of traffic problems")
        |
 CAN / Ethernet vehicle-signal output
```
- Magna states its DMS "can actively detect, predict and react to distracted driving while making allowances for normal actions like checking blind spots" — i.e., the system is explicitly designed with **context-aware false-positive suppression** (a glance to a blind spot is a normal safe-driving action, not distraction), a documented design requirement distinguishing genuine distraction from expected driving behavior.

### 8.2 End-to-end CV pipeline
- **Model families:** not disclosed at the backbone level; Magna's own description (face detect/track from a moving mirror mount, gaze/context-aware distraction classification, plus a growing set of OMS features on the same hardware) is consistent with the shared industry pattern (Part 0.2) — face/landmark CNN + gaze/head-pose regression + a context-aware temporal distraction classifier that must explicitly discriminate "blind-spot check" from "true distraction," implying the temporal model is trained with explicit negative examples of normal driving glances, not just simple gaze-off-road thresholding.
- **Mirror-motion compensation:** because the camera physically moves when the driver adjusts the mirror, Magna's perception pipeline must include either (a) a recalibration step triggered on mirror-position change, or (b) a pose-invariant model formulation — Magna doesn't disclose which, but explicitly frames this as the primary engineering challenge it solved ("the camera must be able to adjust").
- **Testing:** must validate across the "normal action" (blind-spot check, mirror adjustment) vs. "genuine distraction" boundary — an explicitly harder test-set design requirement than simple gaze-off-road-time thresholding.
- **Deployment:** same camera/compute platform serves DMS + OMS + convenience features (video-conferencing, facial recognition) — implying a multi-task model or multiple co-resident models sharing one camera feed and one compute budget.

### 8.3 Cameras, positions, execution architecture, communications
- Primary camera: **frameless interior mirror module** (unusual/distinctive mount point versus most competitors' A-pillar/steering-column/dashboard mounts).
- Secondary application points: center stack, steering wheel.
- LiDAR: N/A for this DMS/OMS pipeline (Part 0.4); Magna's Collective Perception concept references V2X data sharing between vehicles, not LiDAR.
- Comms: Magna is itself a Tier-1 supplying both hardware and integration, so DMS/OMS output would be delivered per the OEM's own E/E architecture (Classic AUTOSAR CAN or Adaptive AUTOSAR SOME/IP/DDS over Ethernet, general pattern, Part 0.2); the "Collective Perception" V2X concept implies an additional **V2X (cellular-V2X / DSRC) communication layer** beyond the in-vehicle network, for inter-vehicle context sharing — a comms layer not present in any other company's DMS architecture reviewed here.

### 8.4 AI training pipeline & deployment
- Not independently disclosed at the framework/hyperparameter level. The explicit design requirement to distinguish normal blind-spot-check glances from genuine distraction strongly implies Magna's training data collection protocol specifically includes labeled "expected/normal" driving-glance sequences (not just distracted vs. attentive), and that the temporal/gaze-zone classifier is trained as a multi-class (or context-conditioned) model rather than a simple binary eyes-on/off-road threshold.
- Deployment spans a family of features (DMS, OMS, child-presence, seatbelt, facial-ID, video-conferencing) built on the same hardware — consistent with a shared-backbone, multi-head deployment architecture, feature-gated per OEM program.

### 8.5 Patents, papers, open-source mapping
- **Company references:** https://www.magna.com/products/exterior-interior/mirrors/driver-monitoring-system (mirror integration, scalability to child-presence/seatbelt/video-conferencing) ; https://ai-online.com/2024/07/the-new-magna-paradigm-for-driver-monitoring-systems/ (Tom Herbert/Tobias Aderum interview — mirror-mount breakthrough, Collective Perception V2X concept, blind-spot-check vs. distraction discrimination)
- Industry-context reference confirming Magna's (and Veoneer's, now part of Magna) position among the ~16 Tier-1 suppliers integrating **Seeing Machines'** perception software specifically (i.e., Magna's own DMS may itself, for some programs, be built on licensed third-party perception software rather than 100% in-house models — consistent with the general Tier-1 hardware + specialist perception-IP pairing pattern seen with Valeo/Seeing Machines): https://www.embedded.com/automotive-focus-shifts-to-driver-monitoring-systems/ and https://www.eetasia.com/driver-monitoring-not-self-driving-is-the-key-auto-market/
- **Patents:** search assignee "Magna Electronics" / "Magna International" / "Veoneer" + "driver monitoring" / "mirror camera" on Google Patents/Espacenet.
- **General literature applicable:** [R6] (driver-behavior-from-camera survey, directly relevant to the normal-glance vs. distraction discrimination problem), [R7] (multi-task attention-monitoring CNN).
- **Open-source analog:** none specific to Magna's mirror-mount architecture; general EAR/PERCLOS + gaze-zone open-source repos (Part 0 appendix) approximate the underlying perception layer only, not the mirror-motion-compensation engineering.

## 9. Mobileye (Mobileye DMS — EyeQ6 Lite / EyeQ6 High, unified with exterior ADAS)

### 9.1 Detailed architecture — hardware, software, computer vision
Mobileye's distinguishing architectural choice is **running DMS on the same EyeQ SoC as the exterior ADAS perception stack**, explicitly eliminating the need for a separate DMS processor/ECU, and **fusing interior gaze data with exterior road-context data** to make smarter distraction/engagement decisions than either sensor alone.

**Hardware:**
- **Infrared cabin camera**, capturing the driver's eyes at **60 frames per second**.
- Compute: **EyeQ6 Lite** (cost-efficient base ADAS SoC) or **EyeQ6 High** (full SuperVision/Surround ADAS SoC) — the same chip family that runs Mobileye's exterior perception stack (lane/object/traffic-light detection, REM crowdsourced mapping, imaging radar fusion). Through 2025, ~230 million vehicles have shipped with EyeQ technology inside.
- Explicitly marketed benefit: **"eliminates the need for integrating a separate processor on their technology stack, reducing costs and supporting SoC/ECU consolidation goals."**

**Software / block diagram:**
```
 IR cabin camera (60 fps eye imaging)              Exterior ADAS cameras + imaging radar
        |                                                      |
   AI neural networks:                                Mobileye exterior perception:
   - Eye-movement / blink-speed analysis                lane detection, object/pedestrian/
   - Gaze tracking, engagement-level scoring             cyclist detection, traffic-light
        |                                                detection, REM crowdsourced map
        +-------------------------+--------------------------+
                                  |
                EyeQ6 Lite / EyeQ6 High single-chip fusion:
                "cross-referencing the driver's gaze with real-time road
                conditions" -> did the driver notice a pedestrian/cyclist/hazard?
                                  |
                Adaptive response: increase following distance, adjust cruise-
                control sensitivity, limit automated lane changes, request gaze
                confirmation before lane change, or issue timely distraction alert
                                  |
                     CAN / Ethernet vehicle-signal + HMI output
```
- Explicit product philosophy: **"assess not just whether a driver is alert, but where they are looking and whether their attention corresponds with what is happening on the road"** — i.e., Mobileye DMS is architected from the outset as a **sensor-fusion problem across interior + exterior perception**, not an isolated interior-only system, which is the clearest documented example among all companies reviewed of gaze-data fused directly with road-scene object detection at the perception-architecture level (rather than only at a downstream rule-engine level).
- In-cabin sensing platform includes both **DMS and Occupant Monitoring (OMS)**, running alongside exterior ADAS perception, on a **single chip**.

### 9.2 End-to-end CV pipeline
- **Model families:** Mobileye describes "AI-powered neural networks" analyzing eye movement and blink speed for gaze/engagement tracking; combined with Mobileye's broader, well-documented computer-vision stack (CNN-based object/lane/traffic-sign/traffic-light detection used across its exterior ADAS products) — the DMS neural network almost certainly shares Mobileye's general CNN-based detection/classification architecture pattern, run as an additional task on the same EyeQ silicon, though Mobileye does not disclose the DMS-specific backbone name.
- **Data creation:** not independently disclosed for the DMS-specific dataset; Mobileye's broader perception stack is trained on very large real-world fleet-collected datasets (well documented for its exterior perception products), and the same large-scale, real-world data-engine culture plausibly extends to its DMS eye/gaze dataset, though this is **industry-inferred**, not company-confirmed at the dataset level.
- **Testing:** the fusion architecture requires validating that gaze-vs-road-context correlation genuinely predicts hazard-awareness — i.e., test scenarios must include labeled "pedestrian/cyclist present + driver noticed / did not notice" ground truth, a more complex test-data requirement than isolated drowsiness/distraction classification.
- **Deployment:** single-chip (EyeQ6 Lite/High) deployment for both interior and exterior perception simultaneously — i.e., the DMS model must fit within the same SoC's power/compute/latency budget as the full exterior ADAS stack, a tight joint-deployment constraint unique to Mobileye's single-chip consolidation strategy among the companies reviewed.

### 9.3 Cameras, positions, execution architecture, communications
- Interior camera: IR, cabin-facing, capturing eyes at 60 fps (position not further specified in public material, but functionally consistent with A-pillar/mirror/cluster-area mounts used industry-wide).
- Exterior: forward-facing high-resolution camera(s), corner parking cameras, imaging radar (up to 11 sensors total processed by a single EyeQ6H in Mobileye's Surround ADAS configuration) — LiDAR is **not** part of Mobileye's core camera-first sensor philosophy (Mobileye has historically emphasized vision-first/"True Redundancy" camera+radar rather than LiDAR-centric sensing for its ADAS products; LiDAR is not mentioned in Mobileye's own DMS or Surround ADAS material reviewed here) (Part 0.4).
- Comms: single-chip architecture reduces the number of ECUs and associated wiring/comms endpoints — Mobileye explicitly frames this as **"eliminating the cost and complexity of a separate DMS ECU."** Signal output (engagement score, gaze-road-correlation result, adaptive-response commands to cruise control/lane-change logic) is distributed over whatever OEM vehicle network integrates the EyeQ SoC — typically CAN/CAN-FD for direct ADAS-actuator commands (following-distance, lane-change permission) and increasingly Automotive Ethernet/SOME-IP for cockpit-domain HMI signals (gaze-confirmation prompts), per the general zonal-architecture pattern (Part 0.2).

### 9.4 AI training pipeline & deployment
- Not independently disclosed in technical depth by Mobileye for the DMS-specific model; company material emphasizes the **system-level training philosophy** of "Compound AI" (a term Mobileye uses generally for its latest-generation perception stack, combining multiple specialized models/reasoning steps) applied to fusing interior gaze data with exterior road-scene understanding.
- Deployment is explicitly tied to specific **production timelines and OEM programs**: e.g., a 2026-announced U.S. automaker production win targets **start of production in 2027**, built on the EyeQ6L SoC, expanding an existing ADAS program's scope — illustrating the typical multi-year automotive production lead time from an architecture/technology decision to shipped vehicles.

### 9.5 Patents, papers, open-source mapping
- **Company references:** https://www.mobileye.com/blog/presenting-the-mobileye-driver-monitoring-system-fusing-road-safety-inside-the-cabin/ (core architecture: 60fps IR eye camera, EyeQ6 Lite/High, gaze-road-context fusion, adaptive response ladder) ; https://www.mobileye.com/news/mobileye-secures-major-dms-production-program-with-leading-u-s-automaker/ and https://www.businesswire.com/news/home/20260323700031/en/Mobileye-Secures-Major-DMS-Production-Program-with-Leading-U.S.-Automaker (2027 SOP production win, single-chip DMS+OMS+ADAS) ; https://www.mobileye.com/news/mobileye-surround-adas-adds-second-top-10-automaker/ (Surround ADAS sensor count/config, EyeQ6H) ; https://www.mobileye.com/press-kit/mobileye/ (EyeQ SoC family overview, REM, imaging radar)
- **Patents:** Mobileye holds an extremely large patent portfolio (parent company Intel/Mobileye) on vision-based ADAS generally; search assignee "Mobileye Vision Technologies" on Google Patents/Espacenet for DMS-specific filings (gaze-road-context fusion, eye-tracking-at-60fps claims specifically were not independently isolated to a single patent number in this research pass).
- **General literature applicable:** [R6] (driver-behavior-from-camera survey, directly relevant to the gaze-vs-hazard-awareness correlation problem Mobileye productized), [R3] (Gaze360 — unconstrained gaze estimation, relevant to real-world eye-tracking-at-scale).
- **Open-source analog:** none specific to Mobileye's proprietary EyeQ stack; the general concept of correlating gaze-zone with detected road hazards is explored academically in driver-attention-prediction literature (e.g., saliency-vs-gaze correlation studies referenced in [R6]'s survey).

## 10. Tobii (Tobii DMS — eye-tracking heritage)

### 10.1 Detailed architecture — hardware, software, computer vision
Tobii enters automotive DMS from a **20-year eye-tracking / attention-computing patent-portfolio heritage** (consumer/scientific eye trackers), positioning its differentiator as population-wide robustness rather than automotive-specific tenure (industry commentary, Section 10.5, notes Tobii "lacks automotive-specific expertise" relative to Seeing Machines/Smart Eye — included here for balance).

**Hardware/software (per Tobii's own product page):**
- Camera: not specified in detail on Tobii's public page beyond "subtle, unobtrusive" cabin-facing sensing; company reinvests **40% of turnover into R&D**.
- Core claim: **"a deep-learning algorithm built on comprehensive population coverage"** — explicitly engineered to work "for any driver, in any situation," including **across hats, sunglasses, face coverings, and all ethnicities** — i.e., Tobii frames its core ML differentiator as **dataset population-coverage breadth** rather than a specific novel model architecture.
- Output signals: distraction, drowsiness, and "other potentially dangerous behaviors," designed to let "OEMs and Tier 1s create warnings and alerts that react to maximum thresholds for driver states **and data from other sensors, like the speedometer**" — i.e., explicit multi-signal fusion (DMS state + vehicle telemetry) at the OEM integration layer.

**Block diagram:**
```
 Cabin camera (subtle/unobtrusive placement) --> Deep-learning population-robust
 model (works across hats/sunglasses/face-coverings/ethnicities)
        |
 Core signals: distraction, drowsiness, dangerous-behavior flags
        |
 OEM/Tier-1 integration layer: fuse DMS state + vehicle telemetry (e.g. speed)
 --> configurable warning/alert thresholds
        |
 CAN / Ethernet vehicle-signal output
```

### 10.2 End-to-end CV pipeline
- **Data creation:** explicitly described as "we collect high-quality data for our deep-learning algorithm, helping us to build an unbeatably robust signal" — company messaging emphasizes **data diversity/coverage** (lighting conditions, hats, sunglasses, face coverings, ethnicities) as the primary lever for robustness, over architecture novelty.
- **Model families:** "deep-learning algorithm" (unspecified backbone) producing the core distraction/drowsiness/dangerous-behavior signals — consistent with, but not more specific than, the shared industry pattern (Part 0.2).
- **Testing:** implied cross-population validation (ethnicity, eyewear, headwear coverage) given the explicit robustness claims — consistent with general best practice for gaze/face CNNs (cross-subject, cross-demographic test splits), matching concerns raised in the broader academic literature ([R16], [R17]) about domain/demographic generalization.
- **Deployment:** positioned as a software/algorithm licensable to OEMs/Tier-1s (similar business model to Smart Eye/Cipia rather than Bosch's/DENSO's full-stack hardware+software model).

### 10.3 Cameras, positions, execution architecture, communications
- Camera: unobtrusive cabin-facing placement (exact mount position not detailed on the public page reviewed).
- LiDAR: N/A (Part 0.4).
- Comms: designed for OEM/Tier-1 integration, fusing DMS state with vehicle telemetry (e.g., speedometer) at the warning-threshold layer — implying the DMS output is consumed alongside standard vehicle CAN signals (speed, steering angle) by a downstream ECU/domain-controller rule engine, consistent with the general Classic-AUTOSAR CAN / Adaptive-AUTOSAR SOME-IP-DDS pattern (Part 0.2).

### 10.4 AI training pipeline & deployment
- Not independently disclosed beyond the population-coverage/data-quality emphasis described above; Tobii's much larger non-automotive eye-tracking patent portfolio (screen-based, VR/AR, assistive-technology eye trackers accumulated over ~20 years) is explicitly cited as underlying automotive DMS IP ("the world's largest eye tracking and attention computing patent portfolio").

### 10.5 Patents, papers, open-source mapping
- **Company references:** https://www.tobii.com/products/automotive/tobii-dms
- **Independent industry commentary** (useful counterpoint/context on competitive positioning across this entire company set): "Automotive focus shifts to driver monitoring systems," Embedded.com, and "Driver Monitoring, Not 'Self-Driving,' is the Key Auto Market," EE Times Asia — both explicitly rank **Seeing Machines and Smart Eye** as the only DMS-software vendors meeting the "decade-plus of automotive R&D" bar, name **Aisin, Mitsubishi** as automotive-established-but-behind-state-of-the-art, and flag **Tobii** as lacking automotive-specific (vs. general eye-tracking) expertise; both also list the ~16 Tier-1s reported to be working with Seeing Machines (Aptiv, Bosch, Continental, Denso, Garmin, Gentex, Harman, Joyson, LG Electronics, Magna, Mitsubishi, Panasonic, Valeo, Veoneer, Visteon, ZF) and Smart Eye's smaller reported Tier-1 list (Aptiv + several Chinese suppliers). https://www.embedded.com/automotive-focus-shifts-to-driver-monitoring-systems/ ; https://www.eetasia.com/driver-monitoring-not-self-driving-is-the-key-auto-market/
- **Patents:** search assignee "Tobii AB" on Google Patents/Espacenet — very large eye-tracking patent portfolio spanning consumer, assistive-tech, and automotive filings.
- **General literature applicable:** [R2], [R3] (MPIIGaze, Gaze360 — foundational large-population appearance-based gaze datasets directly relevant to Tobii's population-coverage claims).
- **Open-source analog:** none specific; general open-source gaze-estimation repos trained on MPIIGaze/Gaze360 (Part 0 appendix) are the closest functional analog.

---

# PART 2 — Cross-Company CV Pipeline, Repository Structure, and Communication Architecture (detailed, generic template applicable to every company above)

## 2.1 Reference monorepo layout
```
dms-platform/
├── data/
│   ├── raw/                       # NIR/RGB video from vehicle logging rigs
│   ├── synthetic/                  # rendered cabin scenes (Bosch-style synthetic pipeline)
│   ├── annotations/                 # landmark, gaze-vector, action-class labels
│   ├── datasets/
│   │   ├── dmd/                    # Driver Monitoring Dataset
│   │   ├── nthu_ddd/                # NTHU Driver Drowsiness Detection dataset
│   │   ├── yawdd/                   # Yawning Detection Dataset
│   │   ├── mrl_eye/                  # MRL Eye Dataset (open/closed eye classification)
│   │   ├── mpiigaze/ , gaze360/        # general appearance-based gaze pretraining data
│   │   └── auc_v1_v2/ , 100-driver/    # distraction/action classification benchmarks
│   └── data_versioning/            # DVC/LakeFS dataset snapshots + dataset cards
├── preprocessing/
│   ├── isp_sim/  face_crop/  augmentation/   # IR-glare, occlusion, sunglasses, motion-blur
├── models/
│   ├── face_detection/             # YOLO-nano / SSD-Mobilenet / RetinaFace-lite
│   ├── landmark_regression/        # HRNet-lite / MobileNet landmark CNN (68-468 pts)
│   ├── head_pose/                   # 6-DoF regression head
│   ├── gaze_estimation/             # ResNet/EfficientNet backbone + gaze-vector head
│   ├── eye_state_perclos/           # MobileNetV3 / ShuffleNetV2 binary/ternary classifier
│   ├── yawn_mouth/                   # MAR-based CNN classifier
│   ├── object_in_hand/               # YOLOv8-nano detection head (phone/cigarette/cup)
│   ├── action_distraction/           # 3D-CNN / two-stream / Transformer-Mamba (DSDFormer-style)
│   ├── temporal_fusion/               # LSTM/GRU/TCN/Transformer sequence model
│   └── multi_task/                    # shared-backbone HyperFace-style joint model
├── training/
│   ├── configs/  train.py  losses/  distillation/  federated/   # incl. FedADAS-style federated KD
├── evaluation/
│   ├── metrics/ (PERCLOS acc, gaze angular error, mAP, ISO 15007 compliance)
│   └── robustness/ (lighting/occlusion/sunglasses/ethnicity/headwear stress tests)
├── export/
│   ├── onnx_export/ tensorrt_build/ qnn_build/ quantization/
├── runtime/
│   ├── isp_pipeline/ inference_server/ (DeepStream/TensorRT/QNN) temporal_state_machine/
│   └── comms/
│       ├── autosar_classic/     # CAN/CAN-FD signal packing via COM/PduR
│       └── autosar_adaptive/     # SOME/IP service defs, DDS topics, ara::com bindings
├── mlops/
│   ├── ci_cd/ model_registry/ ota_pipeline/ monitoring/
├── safety/
│   ├── iso26262_workproducts/ sotif_analysis/
└── docs/
```

## 2.2 Data pipeline (detail applicable across all companies)
1. **Collection** — synchronized NIR+RGB (+CAN ground truth for speed/steering) across day/night/tunnel/backlight/glare, demographics, eyewear, sleep-deprivation protocols (as used for NTHU-DDD/YawDD).
2. **Synthetic augmentation** — Bosch's documented rendering pipeline (Section 3.2) is the clearest company-confirmed example; generalizes to any camera-position/vehicle-type variant.
3. **Annotation** — landmarks (68-pt / MediaPipe 468-pt mesh), gaze target/vector, action/behavior class, object bounding boxes.
4. **Versioning** — DVC/LakeFS snapshotting tied to ISO 26262 traceability work-products.
5. **Class-imbalance handling** — focal loss, hard-negative mining, targeted rare-event (microsleep) collection.

## 2.3 Training pipeline (detail applicable across all companies)
- Frameworks: PyTorch/PyTorch-Lightning or TensorFlow/Keras; Hydra/OmegaConf configs; MLflow/W&B tracking.
- Backbones seen in the public research literature: MobileNetV3, ShuffleNetV2, EfficientNet-B0 [R12]; multi-task HyperFace-style nets [R1]; YOLOv8/YOLO-nano for face/object detection [R11]; DeiT-Tiny (ViT) [R12]; Transformer-Mamba hybrids (DSDFormer) [R10]; LSTM/BiLSTM/GRU temporal models, incl. Cipia's patented anticipatory LSTM (Section 5); 3D-CNN/two-stream/hierarchical temporal deep-belief networks [R9].
- Federated/continual learning: FedADAS-style communication-efficient federated distillation [R11]; DENSO's patented field-feedback retraining loop (Section 7); Cipia's patented online distraction-threshold adaptation (Section 5).
- Loss functions: wing-loss/L2 (landmarks), angular/cosine loss (gaze), focal loss (imbalanced classification), label-smoothed cross-entropy.
- Validation protocol: **mandatory cross-subject (leave-subjects-out) splits** — random-frame splits leak subject identity into gaze/face predictions, a repeatedly flagged pitfall in the literature.

## 2.4 Deployment pipeline (detail applicable across all companies)
1. Export: PyTorch/TF → ONNX.
2. Quantization: PTQ or QAT; TensorRT INT8 (NVIDIA targets, e.g. Jetson AGX Orin used by DSDFormer [R10]); Qualcomm QNN SDK (Snapdragon Ride/Cockpit).
3. Runtime graph: NVIDIA DeepStream (GStreamer-based multi-stream orchestration) + VPI (classical CV ops) alongside DNN inference.
4. Safety wrapping: plausibility checks before signals cross into ASIL-rated network (QM-developed perception + ASIL-rated safety-monitor decomposition, general ISO 26262/SOTIF pattern).
5. OTA updates: signed model packages, staged rollout, rollback, field-telemetry drift monitoring.

## 2.5 Vehicle communication architecture — full detail (applies to every company's vehicle-network integration layer)
- **Classic AUTOSAR path:** DMS status signals (`DrowsinessLevel`, `DistractionWarningActive`) packed as **CAN/CAN-FD** frames through AUTOSAR **COM**/**PduR** stack layers — standard for low-latency, low-bandwidth cluster/haptic alerts. CAN-FD used where higher-rate telemetry exceeds classic CAN's 8-byte/frame, ~1 Mbps ceiling.
- **Adaptive AUTOSAR / Automotive Ethernet path:** raw/compressed camera video to a centralized cockpit-domain computer travels over **automotive Ethernet** (100BASE-T1 to 1000BASE-T1, 100 Mbps–10 Gbps) — CAN bandwidth is inadequate for video.
- **SOME/IP** (AUTOSAR Standard 696): service discovery + RPC/publish-subscribe for DMS-derived state — e.g., an ADAS domain controller *subscribes* to a "DriverAttentionState" service exposed by the cockpit ECU, rather than a static pre-wired CAN signal map.
- **DDS**: alternative/complementary AUTOSAR-Adaptive middleware for high-throughput, low-latency pub-sub, more common where finer QoS control than SOME/IP typically provides is required.
- **Time sync:** **IEEE 802.1AS (gPTP)** keeps sensor timestamps across ECUs in lockstep — necessary to correlate DMS gaze-timing with, e.g., forward-camera lane-departure timing for L2+/L3 arbitration (as in Continental's and Mobileye's fused architectures, Sections 6 & 9).
- **Zonal pattern:** DMS camera(s) connect via **GMSL2/FPD-Link III SerDes** coax to a nearby zone-ECU, which aggregates sensors and forwards over the Ethernet backbone to central compute/HPC.
- **Security:** SOME/IP lacks built-in authentication/encryption in its base spec; production networks add IDPS at the Ethernet-switch layer (e.g., Elektrobit's "EB zoneo SwitchCore Shield") and/or higher-layer TLS/DTLS, given DMS traffic carries biometric-adjacent state.
- **LiDAR note (repeated for completeness):** no company reviewed uses LiDAR for the cabin/driver-facing sensing function; LiDAR appears only in the broader exterior-ADAS sensor suites of Continental (via Ambarella CV3-AD fusion) and Valeo (L3 systems), never in the DMS camera pipeline itself.

---

# PART 3 — Consolidated Silicon Platforms, Datasets, Master Patent/Paper List, Open-Source Map, and Final Caveats

## 3.1 Silicon/compute platforms referenced across companies above
- **NVIDIA Jetson (Orin/Xavier/Nano)**: GMSL2 SerDes → MIPI CSI-2 ingestion (MAX9295 serializer/MAX9296 deserializer reference design), VPI, TensorRT (INT8/FP16), DeepStream multi-stream orchestration. Used as the real-time deployment target in DSDFormer [R10] and the Jetson Nano dual-CNN fatigue detector [R8].
- **Qualcomm Snapdragon Ride/Cockpit**: Hexagon DSP/NPU inference target for cockpit-domain DMS integrations (industry pattern, e.g. Cipia/Jungo/Tobii-class SoC partnerships).
- **Telechips Dolphin SoC** — Continental's Smart Cockpit HPC (Section 6).
- **Ambarella CV3-AD** — Continental's broader AD domain-controller fusion chip, camera+radar+ultrasonic+LiDAR (Section 6).
- **Xilinx/AMD Zynq-7000, then Seeing Machines' own Occula NPU** (Section 1).
- **NXP i.MX8, TI Jacinto (TDA4x)** — frequently cited alternative automotive-qualified (ASIL-B) vision-DSP/NPU SoC families across Tier-1/vendor literature.
- **Mobileye EyeQ6 Lite / EyeQ6 High** — single-chip DMS+OMS+exterior-ADAS fusion (Section 9).

## 3.2 Datasets referenced across the research literature (train/benchmark your own pipeline against these)
DMD (Driver Monitoring Dataset), NTHU-DDD, YawDD, MRL Eye Dataset, AUC Distracted Driver V1/V2, 100-Driver, MPIIGaze, Gaze360 — full citations in Part 0.5.

## 3.3 Master patent list (company-attributed, from this research pass)
- US20210269045A1 — "Contextual driver monitoring system" (Cipia-adjacent; anticipatory LSTM). https://patents.google.com/patent/US20210269045A1/en
- US 12,403,931 B2 — "Vehicular driver monitoring system" (object-in-hand AI training/adaptive thresholds). https://patents.justia.com/patent/12403931
- US20230252759A1 — Denso International America; NN object-detection training-feedback. https://whatafuture.com/denso-smarter-guidebook-driver-monitor
- US 6,927,694 — Denso-relevant early single-camera head/eye-motion alertness algorithm (classical CV baseline). https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/6927694
- US 12,261,920 — AUTOSAR Adaptive Platform dynamic-service-oriented-communication device/method (SOME/IP service/skeleton/proxy pattern relevant to how every company's DMS state services are exposed on Adaptive AUTOSAR). https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/12261920
- For all other companies (Seeing Machines, Smart Eye/Affectiva, Bosch, Valeo, Continental, Magna/Veoneer, Mobileye, Tobii): search the named assignee directly on **Google Patents** (patents.google.com) and **Espacenet** (worldwide.espacenet.com) — portfolios are large (dozens–hundreds of filings each) and best explored directly rather than summarized secondhand.

## 3.4 Master research-paper list
See Part 0.5 [R1]–[R18] for full citations (HyperFace, MPIIGaze, Gaze360, DSDFormer, FedADAS, Human-Centered Benchmarking, Jetson Nano fatigue detection, hierarchical temporal deep-belief network, multi-task attention CNN, dual-camera gaze refinement, NIR CNN gaze detection, etc.) — every paper is cross-referenced from the specific company section(s) whose documented architecture it is most analogous to.

## 3.5 Master open-source map (pipeline stage → repo)
| Stage | Repo |
|---|---|
| Face-mesh + EAR/PERCLOS | `erlendskinnemoen/Driver-Monitoring-Systems-using-Google-s-MediaPipe-FaceMesh` |
| Face-tracker + PERCLOS/MAR | `danielsousaoliveira/driving-monitor-python` |
| Full DMS + DMD-trained action model | `jhan15/driver_monitoring` |
| YOLOv8 + OpenCV + Dlib drowsiness/phone-use | GitHub topic `drowsiness-detection` |
| EAR + head-pose + desktop DMS | GitHub topic `driver-drowsiness-detection` |
| MediaPipe + Streamlit tutorial | https://learnopencv.com/driver-drowsiness-detection-using-mediapipe-in-python/ |
| Jetson Nano dual-CNN (eye+mouth) | [R8] |
| Edge benchmarking harness (MobileNetV3/ShuffleNetV2/EfficientNet-B0/DeiT-Tiny) | [R12] |
| GMSL/MIPI-CSI2 camera ingestion reference | NVIDIA Jetson Linux GMSL docs |

## 3.6 Final caveats (read before treating any diagram above as a literal shipped-code description)
1. **No named company publishes its exact production CNN backbone, layer count, or training hyperparameters.** Everything attributed to a *named company* above is sourced from that company's own public technical/product pages, patents, or press/interview material (all cited inline per section); the CNN/YOLO/ViT/temporal-model *families* are drawn from the general public research literature (Part 0.5) and explicitly labeled "industry-inferred pattern" wherever the company itself has not confirmed the specific model family.
2. **Confirmed exceptions** (companies that *did* explicitly confirm "CNN" or a specific model class in their own material): DENSO+Xperi/FotoNation (2019 Shanghai Motor Show press release explicitly says "convolutional neural network (CNN) based algorithms"); Cipia-adjacent patent family (explicitly names LSTM); Seeing Machines (explicitly says "neural inference processing," without naming a specific backbone).
3. **Thinnest-sourced sections**: Tobii (Section 10) and, to a lesser extent, Magna (Section 8) have less public technical depth than Seeing Machines/Smart Eye/Bosch/DENSO/Cipia/Mobileye; treat their model-architecture claims as more heavily industry-inferred.
4. Where this document states a claim is "industry-inferred," treat it as a reasonable, literature-grounded extrapolation — not a confirmed company-specific fact.
