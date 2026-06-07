#include "comm.h"

#include <mpi.h>

static MPI_Datatype WIRE_T = MPI_DATATYPE_NULL;

static void ensure_type(void) {
  if (WIRE_T != MPI_DATATYPE_NULL)
    return;
  int blocklens[4] = {1, 1, 1, 1};
  MPI_Aint disps[4];
  MPI_Datatype types[4] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};
  wire_msg_t probe = {0, 0, 0, 0};
  MPI_Aint base;
  MPI_Get_address(&probe, &base);
  MPI_Get_address(&probe.ts, &disps[0]);
  MPI_Get_address(&probe.pid, &disps[1]);
  MPI_Get_address(&probe.m, &disps[2]);
  MPI_Get_address(&probe.dock, &disps[3]);
  for (int i = 0; i < 4; ++i)
    disps[i] -= base;
  MPI_Type_create_struct(4, blocklens, disps, types, &WIRE_T);
  MPI_Type_commit(&WIRE_T);
}

static void send_one(const wire_msg_t *m, int to, int tag) {
  ensure_type();
  MPI_Send(m, 1, WIRE_T, to, tag, MPI_COMM_WORLD);
}

void comm_send_req_all(int ts, int my_pid, int m, int dock, int n_procs) {
  wire_msg_t msg = {ts, my_pid, m, dock};
  for (int p = 0; p < n_procs; ++p) {
    if (p == my_pid)
      continue;
    send_one(&msg, p, MSG_REQ);
  }
}

void comm_send_ack(int ts, int my_pid, int to) {
  wire_msg_t msg = {ts, my_pid, 0, 0};
  send_one(&msg, to, MSG_ACK);
}

void comm_send_release_all(int ts, int my_pid, int n_procs) {
  wire_msg_t msg = {ts, my_pid, 0, 0};
  for (int p = 0; p < n_procs; ++p) {
    if (p == my_pid)
      continue;
    send_one(&msg, p, MSG_RELEASE);
  }
}

bool comm_try_recv(int *tag, int *from, wire_msg_t *msg) {
  ensure_type();
  int flag = 0;
  MPI_Status st;
  MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &st);
  if (!flag)
    return false;
  MPI_Recv(msg, 1, WIRE_T, st.MPI_SOURCE, st.MPI_TAG, MPI_COMM_WORLD,
           MPI_STATUS_IGNORE);
  if (tag)
    *tag = st.MPI_TAG;
  if (from)
    *from = st.MPI_SOURCE;
  return true;
}
