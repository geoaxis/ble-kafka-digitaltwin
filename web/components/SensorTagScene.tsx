"use client";
import { Canvas, useFrame, useLoader } from "@react-three/fiber";
import { STLLoader } from "three/examples/jsm/loaders/STLLoader.js";
import { Suspense, useRef } from "react";
import * as THREE from "three";
import type { SensorData } from "@/lib/useSensorData";

function SensorTagModel({ data }: { data: SensorData | null }) {
  const innerRef = useRef<THREE.Group>(null);

  const jacketGeo = useLoader(STLLoader, "/models/jacket.stl");
  const boxGeo = useLoader(STLLoader, "/models/box.stl");

  const target = useRef({ x: 0, y: 0, z: 0 });

  useFrame(() => {
    if (data) {
      const deg2rad = Math.PI / 180;
      target.current.x = -data.p * deg2rad;
      target.current.y = -data.r * deg2rad;
      target.current.z = -data.w * deg2rad;
    }

    if (innerRef.current) {
      const alpha = 0.25;
      innerRef.current.rotation.x += (target.current.x - innerRef.current.rotation.x) * alpha;
      innerRef.current.rotation.y += (target.current.y - innerRef.current.rotation.y) * alpha;
      innerRef.current.rotation.z += (target.current.z - innerRef.current.rotation.z) * alpha;
    }
  });

  return (
    <group rotation={[Math.PI / 2, 0, Math.PI]}>
      <group ref={innerRef}>
        <mesh geometry={jacketGeo} position={[-95.6, -27.6, -102.0]} scale={4}>
          <meshStandardMaterial color="#cc2200" roughness={0.6} metalness={0.0} />
        </mesh>
        <mesh geometry={boxGeo} position={[-73.2, -16.4, -102.0]} scale={4}>
          <meshStandardMaterial color="#ccccdd" roughness={0.4} metalness={0.3} />
        </mesh>
      </group>
    </group>
  );
}

export default function SensorTagScene({ data }: { data: SensorData | null }) {
  return (
    <Canvas
      camera={{ position: [0, 0, 300], near: 1, far: 10000 }}
      style={{ background: "#1a1a2e" }}
    >
      <directionalLight position={[-1, -1, 1]} intensity={2.0} />
      <directionalLight position={[1, 1, 1]} intensity={1.0} />
      <Suspense fallback={null}>
        <SensorTagModel data={data} />
      </Suspense>
    </Canvas>
  );
}
