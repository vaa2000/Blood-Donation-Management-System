# MAINTENANCE MODULE TESTING GUIDE
## Testing Documentation for archiveDailyData() and conditionBasedMaintenance()

**Document Version:** 1.0  
**Last Updated:** December 26, 2025  
**Module:** maintenanceCalculateCount.js

---

## TABLE OF CONTENTS
1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Test Environment Setup](#test-environment-setup)
4. [Testing archiveDailyData Function](#testing-archivedailydata-function)
5. [Testing conditionBasedMaintenance Function](#testing-conditionbasedmaintenance-function)
6. [Integration Testing](#integration-testing)
7. [Troubleshooting](#troubleshooting)
8. [Test Results Documentation](#test-results-documentation)

---

## OVERVIEW

### Functions Under Test

**1. archiveDailyData()**
- **Purpose:** Archives daily maintenance tag counts from historical data
- **Execution:** Scheduled daily at 23:59 via cron job
- **Key Operations:** 
  - Retrieves device maintenance configurations
  - Processes tag data from `periodic-cumulative` table
  - Updates/inserts cumulative counts in `Hist_Count_Archive` table

**2. conditionBasedMaintenance()**
- **Purpose:** Monitors maintenance conditions and sends notifications
- **Execution:** Runs periodically (every few hours) via cron job
- **Key Operations:**
  - Checks tag counts against thresholds
  - Monitors scheduled maintenance dates
  - Sends email alerts and creates alert events

---

## PREREQUISITES

### Required Access
- ✅ Azure Cosmos DB access (Operate-Configuration-DB)
- ✅ PostgreSQL database access (Citus/TimescaleDB)
- ✅ Azure Communication Services (Email service)
- ✅ SignalR hub connection
- ✅ Valid email recipient addresses

### Required Containers (Cosmos DB)
```
Database: Operate-Configuration-DB
Containers:
  - ParameterMappingMaintenance
  - ConfigMaintenance
  - ConfigDevice
  - ConfigTag
  - ConfigAlert
```

### Required Tables (PostgreSQL)
```sql
Tables:
  - public."periodic-cumulative"
  - public."Hist_Count_Archive"
  - public."HistAlerts"
  - public."HistActiveAlerts"
```

### Environment Variables
```bash
Sender_Address=<your-azure-email-sender-address>
```

---

## TEST ENVIRONMENT SETUP

### STEP 1: Verify Database Connections

**Test Cosmos DB Connection:**
```javascript
// Create test file: testCosmosConnection.js
const client = require("./Database/cosmosdb");

async function testCosmosConnection() {
    try {
        const container = client.database("Operate-Configuration-DB")
            .container("ParameterMappingMaintenance");
        const { resources } = await container.items.query("SELECT * FROM c").fetchAll();
        console.log("✅ Cosmos DB Connected. Found", resources.length, "items");
    } catch (err) {
        console.error("❌ Cosmos DB Error:", err.message);
    }
}

testCosmosConnection();
```

**Test PostgreSQL Connection:**
```javascript
// Create test file: testPostgresConnection.js
const { pool } = require("./Database/db/citus");

async function testPostgresConnection() {
    try {
        const res = await pool.query('SELECT NOW()');
        console.log("✅ PostgreSQL Connected:", res.rows[0]);
    } catch (err) {
        console.error("❌ PostgreSQL Error:", err.message);
    }
}

testPostgresConnection();
```

### STEP 2: Prepare Test Data

**A. Create Test Device in Cosmos DB (ParameterMappingMaintenance)**
```json
{
    "id": "TEST_DEVICE_001",
    "DeviceID": "TEST_DEVICE_001",
    "Default_TagID_Threshold": [
        {
            "TagID": "TEST_DEVICE_001.ST.CB.On",
            "Threshold": 100,
            "Enable": true,
            "TagType": "on"
        },
        {
            "TagID": "TEST_DEVICE_001.ST.CB.Trip",
            "Threshold": 50,
            "Enable": true,
            "TagType": "trip"
        }
    ],
    "Assigned_TagID_Threshold": []
}
```

**B. Create ConfigMaintenance Record**
```json
{
    "id": "MAINT_TEST_001",
    "DeviceID": "TEST_DEVICE_001",
    "assigned_by_user": "test@example.com",
    "assigned_to_users": ["technician@example.com"],
    "lastmaintenanceperformeddate": 1703548800,
    "maintenanceintervaldays": 30,
    "nextmaintenancedate": 1706227200,
    "enablealertbeforedays": ["3", "5", "15"]
}
```

**C. Insert Test Data in PostgreSQL (periodic-cumulative)**
```sql
-- Insert historical tag data for today
INSERT INTO public."periodic-cumulative" (tagid, value, timestamp)
VALUES 
    ('TEST_DEVICE_001.ST.CB.On', 5, EXTRACT(EPOCH FROM NOW())::BIGINT - 3600),
    ('TEST_DEVICE_001.ST.CB.On', 8, EXTRACT(EPOCH FROM NOW())::BIGINT - 1800),
    ('TEST_DEVICE_001.ST.CB.On', 12, EXTRACT(EPOCH FROM NOW())::BIGINT - 900),
    ('TEST_DEVICE_001.ST.CB.Trip', 2, EXTRACT(EPOCH FROM NOW())::BIGINT - 3600),
    ('TEST_DEVICE_001.ST.CB.Trip', 3, EXTRACT(EPOCH FROM NOW())::BIGINT - 1800);

-- Verify data inserted
SELECT tagid, value, timestamp, 
       to_timestamp(timestamp) as readable_time 
FROM public."periodic-cumulative" 
WHERE tagid LIKE 'TEST_DEVICE_001%' 
ORDER BY timestamp DESC;
```

**D. Create ConfigAlert for Maintenance (Optional for Full Testing)**
```json
{
    "id": "ALERT_MAINT_TEST_001",
    "TagID": "_TEST_DEVICE_001Maintenance",
    "ProjectID": "TEST_PROJECT",
    "conditions": [
        {
            "alarmpriority": "Highest",
            "value": {
                "alarm_condition_upper": 3,
                "alarm_condition_lower": 0
            },
            "condition": "<=",
            "normalmessage": "Maintenance scheduled",
            "abnormalmessage": "Maintenance due in 3 days"
        },
        {
            "alarmpriority": "High",
            "value": {
                "alarm_condition_upper": 5,
                "alarm_condition_lower": 0
            },
            "condition": "<=",
            "normalmessage": "Maintenance scheduled",
            "abnormalmessage": "Maintenance due in 5 days"
        }
    ]
}
```

---

## TESTING ARCHIVEDAILYDATA FUNCTION

### TEST CASE 1: Basic Archive Functionality

**Objective:** Verify function archives tag counts correctly

**Test Steps:**

1. **Clear Previous Test Data:**
```sql
DELETE FROM public."Hist_Count_Archive" WHERE tagid LIKE 'TEST_DEVICE_001%';
```

2. **Create Test Script:**
```javascript
// testArchiveDaily.js
const { archiveDailyData } = require("./Database/maintenanceCalculateCount");

async function runTest() {
    console.log("🧪 Starting archiveDailyData Test...\n");
    
    try {
        await archiveDailyData();
        console.log("\n✅ Test Completed Successfully");
    } catch (err) {
        console.error("\n❌ Test Failed:", err.message);
    }
    
    process.exit(0);
}

runTest();
```

3. **Execute Test:**
```bash
cd C:\PENTEST_MERGE\SmartComm_Operate_ENERGY_Server\Proprietary\BackEnd-Webapp
node testArchiveDaily.js
```

4. **Verify Results:**
```sql
-- Check if data was archived
SELECT 
    tagid, 
    countvalue, 
    archivedat_timestamp,
    to_timestamp(archivedat_timestamp) as archived_at
FROM public."Hist_Count_Archive"
WHERE tagid LIKE 'TEST_DEVICE_001%';

-- Expected Results:
-- TEST_DEVICE_001.ST.CB.On    | 25  (5+8+12)
-- TEST_DEVICE_001.ST.CB.Trip  | 5   (2+3)
```

**Expected Console Output:**
```
=== Starting Daily Archive - archiveDailyData ===
Archive Period: 2025-12-26T00:00:00.000Z to 2025-12-26T23:59:59.999Z

=== Processing Device: TEST_DEVICE_001 ===
Processing Tag: TEST_DEVICE_001.ST.CB.On, Type: on
Cumulative count for period: 25
Inserted TEST_DEVICE_001.ST.CB.On: Total = 25
Processing Tag: TEST_DEVICE_001.ST.CB.Trip, Type: trip
Cumulative count for period: 5
Inserted TEST_DEVICE_001.ST.CB.Trip: Total = 5
Device TEST_DEVICE_001 Totals - ON: 25, OFF: 0, Trip: 5
=== Daily Archive Complete ===
```

### TEST CASE 2: Cumulative Count Update

**Objective:** Verify function updates existing counts correctly

**Test Steps:**

1. **Run archive first time** (from Test Case 1)

2. **Add more historical data:**
```sql
INSERT INTO public."periodic-cumulative" (tagid, value, timestamp)
VALUES 
    ('TEST_DEVICE_001.ST.CB.On', 10, EXTRACT(EPOCH FROM NOW())::BIGINT - 600),
    ('TEST_DEVICE_001.ST.CB.Trip', 1, EXTRACT(EPOCH FROM NOW())::BIGINT - 600);
```

3. **Run archive again:**
```bash
node testArchiveDaily.js
```

4. **Verify cumulative update:**
```sql
SELECT tagid, countvalue FROM public."Hist_Count_Archive"
WHERE tagid LIKE 'TEST_DEVICE_001%';

-- Expected Results (cumulative):
-- TEST_DEVICE_001.ST.CB.On    | 35  (25 + 10)
-- TEST_DEVICE_001.ST.CB.Trip  | 6   (5 + 1)
```

### TEST CASE 3: State Tag Type (Transition Counting)

**Objective:** Verify state tag correctly counts transitions

**Test Steps:**

1. **Add state-type tag configuration:**
```json
// Add to ParameterMappingMaintenance
{
    "TagID": "TEST_DEVICE_001.ST.CB.State",
    "Threshold": 100,
    "Enable": true,
    "TagType": "state"
}
```

2. **Insert state transition data:**
```sql
-- Simulate ON/OFF transitions
INSERT INTO public."periodic-cumulative" (tagid, value, timestamp)
VALUES 
    ('TEST_DEVICE_001.ST.CB.State', 0, EXTRACT(EPOCH FROM NOW())::BIGINT - 7200), -- OFF
    ('TEST_DEVICE_001.ST.CB.State', 1, EXTRACT(EPOCH FROM NOW())::BIGINT - 6000), -- ON
    ('TEST_DEVICE_001.ST.CB.State', 0, EXTRACT(EPOCH FROM NOW())::BIGINT - 4800), -- OFF
    ('TEST_DEVICE_001.ST.CB.State', 1, EXTRACT(EPOCH FROM NOW())::BIGINT - 3600), -- ON
    ('TEST_DEVICE_001.ST.CB.State', 1, EXTRACT(EPOCH FROM NOW())::BIGINT - 2400), -- Stay ON
    ('TEST_DEVICE_001.ST.CB.State', 0, EXTRACT(EPOCH FROM NOW())::BIGINT - 1200); -- OFF
```

3. **Run archive:**
```bash
node testArchiveDaily.js
```

4. **Verify transition count:**
```sql
SELECT tagid, countvalue FROM public."Hist_Count_Archive"
WHERE tagid = 'TEST_DEVICE_001.ST.CB.State';

-- Expected: countvalue = 2 (two 0→1 transitions)
```

**Expected Console Output:**
```
Processing Tag: TEST_DEVICE_001.ST.CB.State, Type: state
State transitions - ON: 2, OFF: 2
Inserted TEST_DEVICE_001.ST.CB.State: Total = 2
```

### TEST CASE 4: Error Handling

**Objective:** Verify function handles errors gracefully

**Test Steps:**

1. **Test with missing configuration:**
```sql
-- Temporarily remove device from Cosmos DB or delete configuration
```

2. **Run archive:**
```bash
node testArchiveDaily.js
```

3. **Verify error handling:**
- Function should skip device with warning
- Should continue processing other devices
- Should not crash

**Expected Console Output:**
```
=== Processing Device: TEST_DEVICE_001 ===
⚠️  No maintenance config found for device: TEST_DEVICE_001
```

---

## TESTING CONDITIONBASEDMAINTENANCE FUNCTION

### TEST CASE 5: Threshold Breach Detection

**Objective:** Verify function detects when tag count exceeds threshold

**Test Steps:**

1. **Set low threshold in configuration:**
```json
// Update ParameterMappingMaintenance
{
    "TagID": "TEST_DEVICE_001.ST.CB.On",
    "Threshold": 20,  // Lower than current count (35)
    "Enable": true,
    "TagType": "on"
}
```

2. **Ensure archived count exists:**
```sql
-- Verify count is above threshold
SELECT tagid, countvalue FROM public."Hist_Count_Archive"
WHERE tagid = 'TEST_DEVICE_001.ST.CB.On';
-- Should show countvalue >= 20
```

3. **Create test script:**
```javascript
// testConditionBased.js
const { conditionBasedMaintenance } = require("./Database/maintenanceCalculateCount");

async function runTest() {
    console.log("🧪 Starting conditionBasedMaintenance Test...\n");
    
    try {
        await conditionBasedMaintenance();
        console.log("\n✅ Test Completed Successfully");
    } catch (err) {
        console.error("\n❌ Test Failed:", err.message);
    }
    
    process.exit(0);
}

runTest();
```

4. **Execute test:**
```bash
node testConditionBased.js
```

5. **Verify email sent:**
- Check recipient inbox (test@example.com)
- Email subject: "Maintenance Notification"
- Email body should contain:
  - Device Name
  - Tag Name
  - Current Value: 35
  - Threshold Value: 20

**Expected Console Output:**
```
Processing device: TEST_DEVICE_001
Tag TEST_DEVICE_001.ST.CB.On: 35 >= 20 (threshold)
Sending maintenance notification email...
```

### TEST CASE 6: Scheduled Maintenance Alert (3 Days Before)

**Objective:** Verify function sends alert 3 days before maintenance

**Test Steps:**

1. **Set next maintenance date to 3 days from now:**
```javascript
// Calculate timestamp for 3 days from now
const threeDaysFromNow = Math.floor(Date.now() / 1000) + (3 * 24 * 60 * 60);
console.log("Set nextmaintenancedate to:", threeDaysFromNow);
```

2. **Update ConfigMaintenance in Cosmos DB:**
```json
{
    "DeviceID": "TEST_DEVICE_001",
    "nextmaintenancedate": <threeDaysFromNow>,
    "enablealertbeforedays": ["3", "5", "15"],
    "assigned_by_user": "test@example.com"
}
```

3. **Run test:**
```bash
node testConditionBased.js
```

4. **Verify results:**

**A. Check Email:**
- Subject: "Maintenance Notification"
- Body: "Device Name: ..., Days Left: 3"

**B. Check Alert in PostgreSQL:**
```sql
SELECT 
    "AlertID",
    "TagValue",
    "AlertClass",
    "State",
    "TagID",
    to_timestamp("Timestamp"/1000) as alert_time
FROM public."HistAlerts"
WHERE "TagID" = '_TEST_DEVICE_001Maintenance'
ORDER BY "Timestamp" DESC
LIMIT 1;

-- Expected:
-- TagValue: 3
-- AlertClass: 100 (Highest priority)
-- State: active
```

**C. Check Active Alerts:**
```sql
SELECT 
    "AlertID",
    "Abnormal_Message",
    "Normal_Message"
FROM public."HistActiveAlerts"
WHERE "TagID" = '_TEST_DEVICE_001Maintenance'
ORDER BY "Timestamp" DESC
LIMIT 1;

-- Expected:
-- Abnormal_Message: "Maintenance due in 3 days"
```

### TEST CASE 7: Scheduled Maintenance Alert (5 Days Before)

**Objective:** Verify different priority levels work correctly

**Test Steps:**

1. **Update next maintenance date to 5 days from now:**
```javascript
const fiveDaysFromNow = Math.floor(Date.now() / 1000) + (5 * 24 * 60 * 60);
```

2. **Update Cosmos DB**

3. **Clear previous alerts:**
```sql
DELETE FROM public."HistAlerts" WHERE "TagID" = '_TEST_DEVICE_001Maintenance';
DELETE FROM public."HistActiveAlerts" WHERE "TagID" = '_TEST_DEVICE_001Maintenance';
```

4. **Run test:**
```bash
node testConditionBased.js
```

5. **Verify AlertClass:**
```sql
SELECT "AlertClass" FROM public."HistAlerts"
WHERE "TagID" = '_TEST_DEVICE_001Maintenance';

-- Expected: AlertClass = 75 (High priority for 5 days)
```

### TEST CASE 8: Overdue Maintenance Detection

**Objective:** Verify function detects overdue maintenance

**Test Steps:**

1. **Set next maintenance date to past:**
```javascript
const twoDaysAgo = Math.floor(Date.now() / 1000) - (2 * 24 * 60 * 60);
```

2. **Update ConfigMaintenance**

3. **Run test:**
```bash
node testConditionBased.js
```

4. **Verify email:**
- Subject: "Maintenance Notification Date has passed"
- Body: "Days Left: -2" (negative value)

### TEST CASE 9: Disabled Tag Monitoring

**Objective:** Verify disabled tags don't trigger alerts

**Test Steps:**

1. **Disable tag monitoring:**
```json
{
    "TagID": "TEST_DEVICE_001.ST.CB.On",
    "Threshold": 10,
    "Enable": false,  // DISABLED
    "TagType": "on"
}
```

2. **Ensure count exceeds threshold:**
```sql
UPDATE public."Hist_Count_Archive"
SET countvalue = 100
WHERE tagid = 'TEST_DEVICE_001.ST.CB.On';
```

3. **Run test:**
```bash
node testConditionBased.js
```

4. **Verify NO email sent for this tag**

**Expected Console Output:**
```
Tag TEST_DEVICE_001.ST.CB.On: Monitoring disabled, skipping
```

### TEST CASE 10: Multiple Alert Days Configuration

**Objective:** Verify alerts triggered on correct days only

**Test Steps:**

1. **Configure alert days:**
```json
{
    "enablealertbeforedays": ["3", "5", "15", "45"]
}
```

2. **Test each scenario:**

**Scenario A - 10 Days Before (Not in array):**
```javascript
// Set to 10 days from now
const tenDaysFromNow = Math.floor(Date.now() / 1000) + (10 * 24 * 60 * 60);
```
- Expected: NO alert, NO email

**Scenario B - 15 Days Before (In array):**
```javascript
const fifteenDaysFromNow = Math.floor(Date.now() / 1000) + (15 * 24 * 60 * 60);
```
- Expected: Alert generated, email sent
- AlertClass: 50 (Medium priority)

---

## INTEGRATION TESTING

### TEST CASE 11: Full Workflow Integration

**Objective:** Test complete maintenance cycle

**Test Steps:**

1. **Day 1 Morning - Generate historical data:**
```sql
INSERT INTO public."periodic-cumulative" (tagid, value, timestamp)
VALUES 
    ('TEST_DEVICE_001.ST.CB.On', 15, EXTRACT(EPOCH FROM NOW())::BIGINT);
```

2. **Day 1 Evening (23:59) - Run archive:**
```bash
node testArchiveDaily.js
```

3. **Verify archive:**
```sql
SELECT * FROM public."Hist_Count_Archive" 
WHERE tagid = 'TEST_DEVICE_001.ST.CB.On';
-- Expected: countvalue = 15
```

4. **Day 1 Night (00:30) - Run condition check:**
```bash
node testConditionBased.js
```

5. **Day 2 Morning - Add more data:**
```sql
INSERT INTO public."periodic-cumulative" (tagid, value, timestamp)
VALUES 
    ('TEST_DEVICE_001.ST.CB.On', 20, EXTRACT(EPOCH FROM NOW())::BIGINT);
```

6. **Day 2 Evening - Run archive again:**
```bash
node testArchiveDaily.js
```

7. **Verify cumulative:**
```sql
SELECT countvalue FROM public."Hist_Count_Archive" 
WHERE tagid = 'TEST_DEVICE_001.ST.CB.On';
-- Expected: countvalue = 35 (15 + 20)
```

8. **Set threshold to 30 and run condition check:**
```bash
node testConditionBased.js
```

9. **Verify threshold email received** (35 > 30)

### TEST CASE 12: SignalR Alert Broadcasting

**Objective:** Verify alerts broadcast to SignalR clients

**Test Steps:**

1. **Setup SignalR client listener** (in browser console or test client):
```javascript
const connection = new signalR.HubConnectionBuilder()
    .withUrl("/yourSignalRHub")
    .build();

connection.on("ReceiveMessage", (data) => {
    console.log("Received Alert:", data);
    // Verify: data.alerts array contains alert
    // Verify: data.type === "NEW"
});

connection.start();
```

2. **Trigger maintenance alert** (run conditionBased test with proper config)

3. **Verify SignalR message received:**
- Message type: "NEW"
- Alerts array contains maintenance alert
- ProjectID matches

---

## TROUBLESHOOTING

### Issue 1: No Data Archived

**Symptoms:**
- Function runs without errors
- No records in Hist_Count_Archive

**Debugging Steps:**
```sql
-- Check if configuration exists
SELECT * FROM Operate-Configuration-DB.ParameterMappingMaintenance
WHERE DeviceID = 'TEST_DEVICE_001';

-- Check if historical data exists
SELECT COUNT(*) FROM public."periodic-cumulative"
WHERE tagid LIKE 'TEST_DEVICE_001%'
AND timestamp BETWEEN <start> AND <end>;

-- Check function logs
-- Enable console.log statements in archiveDailyData
```

**Common Causes:**
- No configuration in ParameterMappingMaintenance
- No historical data for today
- Tag type mismatch
- Timestamp out of range

### Issue 2: Email Not Received

**Symptoms:**
- Function completes successfully
- No email received

**Debugging Steps:**
```javascript
// Add detailed logging to sendConfiguredEmail
console.log("Email recipient:", recipientsList);
console.log("Email subject:", subject);
console.log("Email message:", emailMessage);
```

**Common Causes:**
- Invalid Sender_Address in .env
- Email service connection issue
- Recipient email typo
- Email in spam folder
- Azure Communication Service quota exceeded

**Verification:**
```javascript
// Test email service independently
const { EmailClient } = require("@azure/communication-email");
const emailclient = new EmailClient(process.env.emailconnectionString);

async function testEmail() {
    const message = {
        senderAddress: process.env.Sender_Address,
        content: {
            subject: "Test Email",
            plainText: "Testing",
        },
        recipients: {
            to: [{ address: "test@example.com" }]
        }
    };
    
    const poller = await emailclient.beginSend(message);
    const result = await poller.pollUntilDone();
    console.log("Email result:", result);
}
testEmail();
```

### Issue 3: Alert Not Created in Database

**Symptoms:**
- Email sent successfully
- No alert record in HistAlerts

**Debugging Steps:**
```sql
-- Check if table exists
SELECT * FROM information_schema.tables 
WHERE table_name = 'HistAlerts';

-- Check table structure
\d public."HistAlerts"

-- Check for SQL errors in logs
```

**Common Causes:**
- Table doesn't exist
- Column name mismatch
- Data type mismatch
- Missing required fields

### Issue 4: Wrong Alert Priority

**Symptoms:**
- Alert created but wrong AlertClass

**Debugging:**
```javascript
// Log priority mapping
console.log("arrayofemailDays:", arrayofemailDays);
console.log("differenceInDays:", differenceInDays);
console.log("Matched condition:", alertsToRaiseConditions[k]);
console.log("AlertClass assigned:", AlertClass);
```

**Common Causes:**
- ConfigAlert conditions misconfigured
- alarm_condition_upper doesn't match enablealertbeforedays
- alertpriority string typo

### Issue 5: Function Crashes

**Symptoms:**
- Function stops with error
- No error handling

**Debugging:**
```javascript
// Wrap in detailed try-catch
try {
    await archiveDailyData();
} catch (err) {
    console.error("Full Error:", err);
    console.error("Stack Trace:", err.stack);
    console.error("Error Name:", err.name);
    console.error("Error Message:", err.message);
}
```

---

## TEST RESULTS DOCUMENTATION

### Test Execution Checklist

**Pre-Test Verification:**
- [ ] All databases accessible
- [ ] Test data created
- [ ] Email service configured
- [ ] Environment variables set

**archiveDailyData Tests:**
- [ ] Test Case 1: Basic Archive - PASS/FAIL
- [ ] Test Case 2: Cumulative Update - PASS/FAIL
- [ ] Test Case 3: State Tag Transitions - PASS/FAIL
- [ ] Test Case 4: Error Handling - PASS/FAIL

**conditionBasedMaintenance Tests:**
- [ ] Test Case 5: Threshold Breach - PASS/FAIL
- [ ] Test Case 6: 3 Days Alert - PASS/FAIL
- [ ] Test Case 7: 5 Days Alert - PASS/FAIL
- [ ] Test Case 8: Overdue Maintenance - PASS/FAIL
- [ ] Test Case 9: Disabled Monitoring - PASS/FAIL
- [ ] Test Case 10: Multiple Alert Days - PASS/FAIL

**Integration Tests:**
- [ ] Test Case 11: Full Workflow - PASS/FAIL
- [ ] Test Case 12: SignalR Broadcasting - PASS/FAIL

### Results Template

```
TEST EXECUTION REPORT
=====================
Date: ________________
Tester: ______________
Environment: _________

Test Case: ___________
Expected Result: ______________________________
Actual Result: ________________________________
Status: PASS / FAIL
Notes: _________________________________________

Screenshots/Logs Attached: YES / NO
```

### Performance Metrics

**Measure and Document:**
- Execution time for archiveDailyData
- Execution time for conditionBasedMaintenance
- Number of devices processed
- Number of tags processed
- Number of emails sent
- Number of alerts created

**Sample Metrics Collection:**
```javascript
const startTime = Date.now();
await archiveDailyData();
const endTime = Date.now();
console.log(`Execution Time: ${endTime - startTime}ms`);
```

---

## QUICK REFERENCE COMMANDS

### Run Functions Manually

```bash
# Navigate to backend directory
cd C:\PENTEST_MERGE\SmartComm_Operate_ENERGY_Server\Proprietary\BackEnd-Webapp

# Test archiveDailyData
node -e "require('./Database/maintenanceCalculateCount').archiveDailyData().then(() => process.exit())"

# Test conditionBasedMaintenance
node -e "require('./Database/maintenanceCalculateCount').conditionBasedMaintenance().then(() => process.exit())"
```

### Database Query Shortcuts

```sql
-- View all archived counts
SELECT * FROM public."Hist_Count_Archive" ORDER BY archivedat_timestamp DESC LIMIT 10;

-- View recent alerts
SELECT * FROM public."HistAlerts" ORDER BY "Timestamp" DESC LIMIT 10;

-- View active alerts
SELECT * FROM public."HistActiveAlerts" WHERE "State" = 'active';

-- Count tags by device
SELECT 
    SUBSTRING(tagid FROM 1 FOR POSITION('.' IN tagid)-1) as device,
    COUNT(*) as tag_count
FROM public."Hist_Count_Archive"
GROUP BY device;
```

### Cosmos DB Query Shortcuts

```sql
-- View maintenance configurations
SELECT * FROM c WHERE c.DeviceID LIKE 'TEST_%'

-- View alert configurations
SELECT c.TagID, c.conditions FROM c WHERE c.TagID LIKE '_% Maintenance'

-- View parameter mappings
SELECT c.DeviceID, ARRAY_LENGTH(c.Default_TagID_Threshold) as tag_count FROM c
```

---

## CONCLUSION

This testing guide provides comprehensive procedures for validating the maintenance module functionality. Follow each test case systematically and document all results. For production deployment, ensure all test cases pass before enabling the cron jobs.

**Next Steps:**
1. Complete all test cases
2. Document any issues found
3. Verify performance metrics
4. Schedule cron jobs if all tests pass
5. Monitor production execution logs

**Support:**
- Review code comments in maintenanceCalculateCount.js
- Check Azure portal for email service logs
- Monitor PostgreSQL slow query logs
- Review SignalR connection logs
