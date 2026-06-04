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
    }
}
