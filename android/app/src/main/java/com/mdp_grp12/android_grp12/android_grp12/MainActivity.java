package com.mdp_grp12.android_grp12.android_grp12;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.widget.ImageButton;

import com.mdp_grp12.android_grp12.R;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.home_page);

        ImageButton arena_btn = findViewById(R.id.arena_btn);
        ImageButton bluetooth_btn = findViewById(R.id.bt_btn);
        ImageButton msg_btn = findViewById(R.id.msg_btn);

        // On-click listeners
        arena_btn.setOnClickListener(v -> openArenaView());
        bluetooth_btn.setOnClickListener(v -> openBTView());
        msg_btn.setOnClickListener(v -> openMsgView());

//        // Use LayoutInflater to get other views (arena)
//        LayoutInflater myLayoutInflater = getLayoutInflater();
//        View arenaView = myLayoutInflater.inflate(R.layout.arena, null);
    }

    public void openArenaView() {
        Intent intent = new Intent(this, Arena.class);
        startActivity(intent);
    }

    public void openBTView() {
        Intent intent = new Intent(this, Bluetooth.class);
        startActivity(intent);

    }

    public void openMsgView() {
        Intent intent = new Intent(this, Message.class);
        startActivity(intent);
    }
}