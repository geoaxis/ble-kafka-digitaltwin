"use client";
import dynamic from "next/dynamic";
import { useSensorData } from "@/lib/useSensorData";

const SensorTagScene = dynamic(
  () => import("@/components/SensorTagScene"),
  { ssr: false }
);

export default function Home() {
  const { data, connected, latency } = useSensorData("ws://localhost:3001");

  return (
    <div className="h-screen w-screen bg-[#1a1a2e] flex flex-col">
      {/* 3D viewport */}
      <div className="flex-1">
        <SensorTagScene data={data} />
      </div>

      {/* Debug overlay */}
      <div className="absolute top-4 left-4 bg-black/70 rounded-lg p-3 font-mono text-xs space-y-1">
        <div className={connected ? "text-green-400" : "text-red-400"}>
          {connected ? "● CONNECTED" : "○ DISCONNECTED"}
        </div>
        {data && (
          <>
            <div className="text-yellow-300">
              P: {data.p.toFixed(1)}° &nbsp; R: {data.r.toFixed(1)}° &nbsp; Y: {data.w.toFixed(1)}°
            </div>
            <div className="text-white">
              Gyro  X:{data.gx.toFixed(1)} Y:{data.gy.toFixed(1)} Z:{data.gz.toFixed(1)} °/s
            </div>
            <div className="text-white">
              Accel X:{data.ax.toFixed(2)} Y:{data.ay.toFixed(2)} Z:{data.az.toFixed(2)} g
            </div>
            <div className="text-blue-300">
              Latency: {latency}ms
            </div>
          </>
        )}
        {!data && <div className="text-gray-400">Waiting for sensor data...</div>}
      </div>

      {/* Bottom info bar */}
      <div className="h-16 bg-[#16213e] border-t-2 border-[#0f3460] flex items-center justify-center">
        <span className="text-[#4fc3f7] font-bold text-lg">
          SensorTag Digital Twin
        </span>
      </div>
    </div>
  );
}
