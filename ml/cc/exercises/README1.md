# DMS Perception Enterprise Reference Implementation — Detailed Design

## 1. Purpose and scope

This repository is an end-to-end Driver Monitoring System architecture demonstrator. Every block in the requested sensing → ISP → perception → temporal → decision/safety → ECU/HMI architecture has an executable implementation, an explicit adapter, or a deterministic simulator when specialized automotive hardware/vendor SDKs are unavailable.

It is designed as a reference for architecture study, algorithm prototyping, training-pipeline development, HIL integration and embedded deployment preparation. It is not represented as an ISO 26262 ASIL-certified production item. ASIL capability requires a safety lifecycle, HARA, FSC/TSC, safety requirements traceability, dependent-failure analysis, tool qualification, hardware metrics and verification evidence.

## 2. Top-level architecture

The executable entry point is `main.py`. The runtime data flow is:

Camera/video → SensingLayer → ISP → Perception → TemporalEnsemble → DecisionEngine → SafetyMonitor → CAN/UDP/HMI.

The requested industry-style source hierarchy is also present under `src/`. Runtime modules under `dms/` are kept small for a one-command demo; `src/` exposes domain-oriented adapters suitable for decomposition into production components.

## 3. Sensing layer

`dms/sensing.py` and `src/sensing/sensor_manager.py` own sensor acquisition.

RGB input is captured with OpenCV. The same interface accepts a video file. NIR is simulated from luminance and local contrast when no IR sensor is present. This preserves the IR frame contract and allows the rest of the pipeline to run unchanged.

ToF/depth is represented by a monocular depth proxy. Radar occupancy is simulated as a boolean occupancy signal. Steering torque, grip, seat pressure and heart rate are deterministic time-varying sensor simulators. All signals are packaged with a monotonic timestamp.

A production replacement would bind V4L2/GMSL camera drivers, IR LED current control, ToF SDK, radar CAN/SOME-IP signals, steering ECU signals, seat ECU signals and wearable/BLE data to the same contract.

## 4. ISP and preprocessing

`dms/isp.py` implements resize, percentile-based IR auto-exposure normalization, fast non-local-means denoising, camera-undistortion using an intrinsic matrix/distortion vector, timestamped RGB/NIR buffering and aligned frame output.

The current distortion coefficients are zero because a generic webcam has no supplied calibration file. The API is already structured to consume calibrated K/D matrices.

ROI processing occurs after face localization. Face, eye and hand/activity regions are derived from detections/landmarks. Region normalization is performed by resize and normalized model inputs.

## 5. Stage 1 — face and person detection

`src/detection/detectors.py` defines the detector boundary. The runnable fallback uses OpenCV Haar face detection and HOG person detection. The contract is intentionally backend-neutral so YOLO-tiny, SSD-MobileNet, RetinaFace or BlazeFace can replace it.

The runtime additionally fuses seat-pressure and radar-occupancy evidence with face visibility. This prevents the driver-availability state from relying on a single camera cue.

## 6. Stage 2 — landmarks, head pose and eye processing

`dms/models.py` contains a trainable `LandmarkCNN` producing 98 normalized points and an `EyeCNN` producing two eye outputs. `src/landmarks/networks.py` exposes these networks.

For a no-checkpoint demo, `dms/perception.py` generates a dense 98-point face geometry and overwrites semantic eye, nose, chin and mouth indices. This ensures all downstream geometry is executable before training.

Six-degree-of-freedom head pose is solved with OpenCV `solvePnP`. The outputs are yaw, pitch, roll and translation x/y/z.

Eye aperture is measured using eye aspect ratio. Pupil/iris localization uses a dark-region image estimator inside each eye ROI. `EyeCNN` is an executable PyTorch branch and can be trained/replaced by a calibrated eyelid-aperture network.

## 7. Stage 3 — gaze and action recognition

`GazeFusionNet` is an appearance/geometry fusion architecture. It contains an eye image encoder, face image encoder and geometry feature branch. The geometry branch accepts pupil coordinates and head pose.

The runnable fallback calculates gaze direction from binocular pupil displacement plus head-pose contribution. `GazeZoneClassifier` maps the resulting direction to ROAD, LEFT, RIGHT, UP or DOWN.

`ActivityViT` is a ViT-style driver-activity branch. A convolutional patch embedder converts the cabin image into tokens; a Transformer encoder models global activity context.

The canonical action labels are phone, smoking, drinking, hands-off-wheel and reaching. In demo mode, these probabilities are derived from observable head/mouth cues and simulated grip data. This avoids pretending randomly initialized neural weights are trained detectors.

A production training run should use COCO/OpenLABEL annotations and trained YOLO object heads for phone/cigarette/cup/hand objects, with the ViT branch handling higher-order activity context.

## 8. Stage 4 — temporal and sequence processing

`dms/temporal.py` implements the rule-based temporal state machine and neural temporal ensemble.

The state machine calculates PERCLOS, blink rate, yawn persistence and off-road-gaze persistence. It uses bounded delta time to avoid a debugger pause or frame stall corrupting temporal state.

Five executable temporal architectures are implemented: LSTM, GRU, convolution-plus-LSTM, dilated 1D-CNN/TCN and Transformer encoder.

Each consumes a `[batch, time, 20]` sequence. The feature vector contains eye aperture, pupil coordinates, normalized head pose, gaze zone, activity probabilities, mouth openness, perception confidence, grip, heart rate and depth.

Without trained checkpoints, neural predictions are deliberately assigned low fusion weight. Rule/sensor evidence remains dominant. `training/train.py` provides a runnable synthetic-data trainer with structured class patterns so the Transformer branch can learn a real mapping rather than random labels.

## 9. Stage 5 — fusion, decision and intervention

`dms/decision.py` performs multimodal confidence fusion at the state level. Drowsiness and distraction scores are compared, persistence timers are accumulated and a hysteretic state transition policy prevents alert chatter.

The states are NORMAL, ADVISORY, WARNING, CRITICAL and MRM.

Critical persistence generates a hand-back request. Longer critical persistence generates an MRM request. Recovery requires sustained low-risk evidence before downgrade.

The live HMI is an OpenCV overlay. Warning states generate a terminal bell. Production visual/audio/haptic actuators would be connected at the same intervention boundary.

## 10. Functional-safety monitor

`dms/safety.py` and `deployment/safety_monitor/` implement an independent monitor boundary. Checks include finite-number validation, sensor-range plausibility, signal freshness, perception confidence range, latency deadline and heartbeat/watchdog support.

A safety fault overrides the functional decision with at least WARNING and reason `DMS_SAFETY_FAULT`.

This is a safety architecture pattern, not an ASIL claim. In production, the monitor should execute with sufficient independence, have allocated safety requirements and be verified according to the target ASIL decomposition.

## 11. CAN, Ethernet and ECU runtime

`dms/output.py` packs an eight-byte simulated CAN payload at arbitration ID `0x5A0`. It carries alert level, driver availability, hand-back, MRM, drowsiness and distraction.

The same state is emitted as UDP JSON. `tools/udp_receiver.py` is a runnable Ethernet receiver.

`deployment/ecu_runtime/` contains an ECU runtime facade, an ara::com boundary adapter and a SOME/IP demo adapter. Real AUTOSAR generated bindings, DBC definitions and OEM SOME/IP stack are platform inputs and are intentionally not fabricated.

## 12. Data and annotation architecture

`data/raw` is the immutable capture landing area. `data/annotations` documents the canonical COCO/OpenLABEL contract. `data/synthetic/generate.py` creates runnable synthetic NIR-like face frames. `data/splits/make_subject_splits.py` creates subject-independent train/validation/test partitions.

Subject-independent splitting is mandatory for credible DMS evaluation because random frame splitting leaks driver identity and session appearance.

## 13. Training and experiment tracking

`training/configs/enterprise.yaml` centralizes seed, data split, augmentation, optimizer and quality gates.

`training/train.py` is a runnable PyTorch training pipeline. The synthetic fallback creates learnable patterns for normal, drowsy, distracted and hands-off/reaching states. Model artifacts are written under `training/artifacts`.

Experiment history is persisted to `training/experiment_tracking/run.json`. The interface can be replaced with MLflow or Weights & Biases without modifying model code.

## 14. Export and quantization

`export/onnx_export.py` exports LSTM, GRU, ConvLSTM-style, TCN and Transformer branches to ONNX.

`export/quantization/ptq.py` generates an INT8 PTQ calibration manifest. `qat.py` prepares PyTorch models for QAT.

Backend directories document the integration boundary for TensorRT, Ambarella CVflow, Qualcomm QNN, TI TIDL and NXP eIQ. Proprietary compiler SDKs are not redistributed.

## 15. OTA model packaging

`deployment/ota/package_model.py` computes SHA-256, creates a versioned manifest, records rollback/self-test requirements and packages the model.

A production implementation must add asymmetric manifest signing, HSM-backed keys, secure boot integration, anti-rollback counters and atomic A/B deployment.

## 16. Validation and HIL

`validation/metrics/metrics.py` implements PERCLOS tolerance accuracy, gaze RMSE, false-accept rate and false-reject rate.

`validation/scenario_replay/replay.py` executes the same production-shaped pipeline over recorded scenarios.

`validation/hil_bench/hil.py` records latency samples and reports p50/p95/p99 latency. A real HIL bench should inject synchronized camera, CAN and Ethernet data and verify alert timing, watchdog behavior, thermal throttling and degraded modes.

## 17. CI/CD and regression gating

`ci_cd/pipelines.yaml` installs dependencies, executes tests, generates synthetic data, evaluates metrics and exports ONNX.

The enterprise configuration defines target gates for gaze RMSE, FAR, drowsiness F1 and p95 latency. Production CI should fail the release job when any approved baseline slice regresses.

## 18. Runtime and demonstration procedure

Create a Python 3.11 virtual environment, install `requirements.txt`, and run `python main.py --source 0`.

The window displays driver state, gaze, head pose, PERCLOS, temporal scores, activity probabilities, simulated vehicle/biometric signals, hand-back/MRM status and the CAN payload.

Press N to switch between RGB and simulated NIR visualization. Press ESC to exit.

Run `python tools/udp_receiver.py` in a second terminal to observe Ethernet output.

Run `python data/synthetic/generate.py`, then `python training/train.py` to execute the training pipeline. Run `python export/onnx_export.py` to create ONNX models.

## 19. Productionization gaps

The repository covers every architectural block, but simulated hardware and untrained fallback estimators must be replaced for a commercial DMS. Required inputs include calibrated 850/940 nm cameras and illuminators, optical/eye-safety design, trained NIR face/landmark/eye/gaze/activity checkpoints, target SoC SDKs, OEM vehicle signal definitions, large consented driver datasets, regulatory scenario coverage and ISO 26262 safety work products.

The architectural boundaries are designed so these replacements do not require rewriting the complete pipeline.
