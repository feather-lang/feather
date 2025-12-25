/**
 * TypeScript declarations for feather JavaScript host
 */

export declare const TCL_OK: 0;
export declare const TCL_ERROR: 1;
export declare const TCL_RETURN: 2;
export declare const TCL_BREAK: 3;
export declare const TCL_CONTINUE: 4;

export type TclResultCode = typeof TCL_OK | typeof TCL_ERROR | typeof TCL_RETURN | typeof TCL_BREAK | typeof TCL_CONTINUE;

export interface TclError extends Error {
  code: TclResultCode;
}

export type HostCommand = (args: string[]) => string | number | void;

export interface ForeignTypeDef {
  methods?: Record<string, (value: any, ...args: string[]) => any>;
  destroy?: (value: any) => void;
}

export interface Feather {
  /**
   * Create a new interpreter instance.
   * @returns The interpreter ID
   */
  create(): number;

  /**
   * Register a host command that can be called from TCL scripts.
   * @param interpId The interpreter ID
   * @param name The command name
   * @param fn The function to execute when the command is called
   */
  register(interpId: number, name: string, fn: HostCommand): void;

  /**
   * Register a foreign type that can be instantiated from TCL.
   * @param interpId The interpreter ID
   * @param typeName The type name (e.g., "Mux", "Connection")
   * @param typeDef The type definition with methods and optional destructor
   */
  registerType(interpId: number, typeName: string, typeDef: ForeignTypeDef): void;

  /**
   * Create a foreign object wrapping a host value.
   * @param interpId The interpreter ID
   * @param typeName The type name
   * @param value The host value to wrap
   * @param stringRep Optional string representation
   * @returns The object handle
   */
  createForeign(interpId: number, typeName: string, value: any, stringRep?: string): number;

  /**
   * Evaluate a TCL script.
   * @param interpId The interpreter ID
   * @param script The TCL script to evaluate
   * @returns The result string
   * @throws {TclError} If evaluation fails
   */
  eval(interpId: number, script: string): string;

  /**
   * Get the current result from the interpreter.
   * @param interpId The interpreter ID
   * @returns The result string
   */
  getResult(interpId: number): string;

  /**
   * Destroy an interpreter instance.
   * @param interpId The interpreter ID
   */
  destroy(interpId: number): void;

  /**
   * Access to the raw WASM exports.
   */
  readonly exports: WebAssembly.Exports;
}

/**
 * Create a feather interpreter host from a WASM module.
 * @param wasmSource Path to .wasm file, URL, ArrayBuffer, or Response
 * @returns A Feather instance
 */
export declare function createFeather(
  wasmSource: string | ArrayBuffer | ArrayBufferView | Response
): Promise<Feather>;
