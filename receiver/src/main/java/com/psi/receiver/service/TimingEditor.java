package com.psi.receiver.service;

import org.json.JSONException;
import org.json.JSONObject;
import org.springframework.stereotype.Service;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

@Service
public class TimingEditor {
    public void combineJson(Path basePath, JSONObject baseJson, Path senderPath) throws IOException, JSONException {
        String senderContent = new String(Files.readAllBytes(senderPath));
        JSONObject senderJson = new JSONObject(senderContent);

        baseJson.put("sender", senderJson);

        String indented = baseJson.toString(4);

        Files.write(
                basePath,
                indented.getBytes("UTF-8"),
                StandardOpenOption.CREATE,
                StandardOpenOption.TRUNCATE_EXISTING
        );

        System.out.println("[timing] storage/timing.json saved");



//        String senderJson = Files.exists(senderJson)
//                ? Files.readString(senderJson).trim()
//                : "{}";
//
//        String json = String.format(
//                "{%n" +
//                        "  \"receiver\": {%n" +
//                        "    \"phase1_request\": {%n" +
//                        "      \"hashingMs\": %.3f,%n" +
//                        "      \"windowingMs\": %.3f%n" +
//                        "    },%n" +
//                        "    \"phase2_transfer\": {%n" +
//                        "      \"transferMs\": %.3f%n" +
//                        "    },%n" +
//                        "    \"phase3_result\": {%n" +
//                        "      \"loadMs\": %.3f,%n" +
//                        "      \"decryptMs\": %.3f,%n" +
//                        "      \"intersectMs\": %.3f%n" +
//                        "    }%n" +
//                        "  },%n" +
//                        "  \"sender\": %s%n" +
//                        "}",
//                time.getHashingMs(), time.getWindowingMs(),
//                time.getTransferMs(),
//                time.getLoadMs(), time.getDecryptMs(), time.getIntersectMs(),
//                senderJson
//        );
//        Files.createDirectories(STORAGE_DIR);
//        Files.writeString(TIMING_JSON, json);
//        System.out.println("[timing] storage/timing.json 저장 완료");
    }
}
