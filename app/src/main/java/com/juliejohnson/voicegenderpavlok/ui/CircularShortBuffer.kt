package com.juliejohnson.voicegenderpavlok.ui

class CircularShortBuffer(private val capacity: Int) {
    private val buffer = ShortArray(capacity)
    private var index = 0
    private var isFull = false

    fun append(data: ShortArray) {
        for (value in data) {
            buffer[index] = value
            index = (index + 1) % capacity
            if (index == 0) isFull = true
        }
    }

    fun toArray(): ShortArray {
        return if (!isFull) buffer.copyOfRange(0, index)
        else buffer.copyOfRange(index, capacity) + buffer.copyOfRange(0, index)
    }
}
