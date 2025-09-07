package com.mdp_grp12.android_grp12.android_grp12;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.util.Pair;
import android.view.View;

import java.util.ArrayList;
import java.util.List;

public class CanvasGrid extends View {
    private static final int noOfCols = 20;
    private static final int noOfRows = 20;
    private static final int cellWidth = 35;
    private static final int cellHeight = 35;

    private static final Paint greenPaint = new Paint();
    private static final Paint blackPaint = new Paint();
    private static final Paint whitePaint = new Paint();
    private static final Paint canvasBackground = new Paint();
    private static final Paint highlightPaint = new Paint();

    private int highlightedRow = -1;
    private int highlightedCol = -1;

    private int carX = -1;
    private int carY = -1;
    private int highlightRadius = 0; // Set the default radius to 1
    private static List<Pair<Integer, Integer>> highlightPositions = new ArrayList<>();


    public CanvasGrid(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);

        String backgroundColor = "#000000";
        String gridColor = "#03BFB5";

        blackPaint.setColor(Color.BLACK);
        whitePaint.setColor(Color.WHITE);
        greenPaint.setColor(Color.parseColor(gridColor));
        canvasBackground.setColor(Color.parseColor(backgroundColor));

        highlightPaint.setColor(Color.parseColor("#03BFB5")); // Bluish-Green color for highlighting
        highlightPaint.setAlpha(70); // Set alpha for transparency
    }

    public void highlightCell(int row, int col) {
        highlightedRow = row;
        highlightedCol = col;
        invalidate(); // Request a redraw to update the highlighting
    }

    public void clearHighlight() {
        highlightedRow = -1;
        highlightedCol = -1;
        clearPositions();
        invalidate(); // Request a redraw to clear the highlight
    }

    public void resetCarXY () {
        carX = -1;
        carY = -1;
    }

    public void setCarPosition(int x, int y) {
        carX = x;
        carY = y;

        // Add the car's position and surrounding positions to highlight
        for (int i = -highlightRadius; i <= highlightRadius; i++) {
            for (int j = -highlightRadius; j <= highlightRadius; j++) {
                int highlightX = carX + i;
                int highlightY = carY + j;
                if (highlightX >= 0 && highlightY >= 0 && highlightX < noOfCols && highlightY < noOfRows) {
                    highlightPositions.add(new Pair<>(highlightX, highlightY));
                    Log.d("Debug -> GridHighlight", "Highlight position: X = " + highlightX + ", Y = " + highlightY);

                }
            }
        }

        invalidate(); // Request a redraw to update the car's position
    }

    public void setCarPosition2(int x, int y) {
        int prevX = carX; // Store the previous X position
        int prevY = carY; // Store the previous Y position

        carX = x;
        carY = y;

        // Add the car's position and surrounding positions to highlight
        for (int i = -highlightRadius; i <= highlightRadius; i++) {
            for (int j = -highlightRadius; j <= highlightRadius; j++) {
                int highlightX = carX + i;
                int highlightY = carY + j;
                if (highlightX >= 0 && highlightY >= 0 && highlightX < noOfCols && highlightY < noOfRows) {
                    // Highlight the path from the previous position to the new position
                    if (prevX == -1 && prevY == -1) {
                        prevX = 1;
                        prevY = 18;
                    }
                        // Highlight rows in the path
                        for (int k = Math.min(prevX, highlightX) + 1; k < Math.max(prevX, highlightX); k++) {
                            highlightPositions.add(new Pair<>(k, prevY));
                            Log.d("Debug -> GridHighlight", "Highlight position: X = " + k + ", Y = " + prevY);
                        }
                        // Highlight columns in the path
                        for (int k = Math.min(prevY, highlightY) + 1; k < Math.max(prevY, highlightY); k++) {
                            highlightPositions.add(new Pair<>(highlightX, k));
                            Log.d("Debug -> GridHighlight", "Highlight position: X = " + highlightX + ", Y = " + k);

                        }

                    // Add the highlight position
                    highlightPositions.add(new Pair<>(highlightX, highlightY));
                    Log.d("Debug -> GridHighlight", "Highlight position: X = " + highlightX + ", Y = " + highlightY);
                }
            }
        }

        // Call invalidate here to update the UI with the new highlight positions
        invalidate();
    }

    public void setHighlightRadius (int rad) {
        this.highlightRadius = rad;
    }

    public void clearPositions() {
        highlightPositions.clear();
    }

    @Override
    public void onDraw(Canvas canvas) {
        // Background
        for (int i = 0; i < noOfCols; i++) {
            for (int j = 0; j < noOfRows; j++) {
                canvas.drawRect(i * cellWidth, j * cellHeight, (i + 1) * cellWidth, (j + 1) * cellHeight,
                        canvasBackground);
            }
        }

        // Vertical lines
        for (int i = 1; i < noOfCols; i++) {
            canvas.drawLine(i * cellWidth, 0, i * cellWidth, noOfRows * cellHeight, greenPaint);
        }

        // Horizontal lines
        for (int i = 1; i < noOfCols; i++) {
            canvas.drawLine(0, i * cellHeight, noOfCols * cellWidth, i * cellHeight, greenPaint);
        }

        // Highlight the respective row and column
        if (highlightedRow >= 0) {
            int highlightTop = highlightedRow * cellHeight;
            int highlightBottom = (highlightedRow + 1) * cellHeight;

            canvas.drawRect(new Rect(0, highlightTop, noOfCols * cellWidth, highlightBottom), highlightPaint); // Highlight row
        }

        if (highlightedCol >= 0) {
            int highlightLeft = highlightedCol * cellWidth;
            int highlightRight = (highlightedCol + 1) * cellWidth;

            canvas.drawRect(new Rect(highlightLeft, 0, highlightRight, noOfRows * cellHeight), highlightPaint); // Highlight column
        }

        // Vertical grid axis
        for (int i = noOfRows - 1; i >= 0; i--) {
            canvas.drawText(String.valueOf(i), 0, cellHeight * (noOfRows - i - 1) + 15, greenPaint);
        }

        // Horizontal grid axis
        for (int i = 1; i < noOfCols; i++) {
            canvas.drawText(String.valueOf(i), cellWidth * i + 5, cellHeight * 10 + 15, greenPaint);
        }

        // Highlight positions stored in the list
        for (Pair<Integer, Integer> position : highlightPositions) {
            int highlightX = position.first;
            int highlightY = position.second;
            int highlightLeft = highlightX * cellWidth;
            int highlightTop = highlightY * cellHeight;
            int highlightRight = (highlightX + 1) * cellWidth;
            int highlightBottom = (highlightY + 1) * cellHeight;

            canvas.drawRect(new Rect(highlightLeft, highlightTop, highlightRight, highlightBottom), highlightPaint);
            Log.d("Draw", highlightLeft + ", " + highlightTop + ", " + highlightRight + ", " + highlightBottom);
        }
    }

}
